// Copyright 2008, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// This file implements GearsGeolocation, the main class of the Gears
// Geolocation API.
//
// Most of the methods of this class should only be called on the JavaScript
// thread. The exceptions are LocationUpdateAvailable and OnTimeout, which
// immediately marshall the call to the JavaScript thread using messages.
//
// This means that the class does not need to be thread safe. However, it does
// need to be reentrant, because the JavaScript callback may call back into
// Gears code. Furthermore, if a callback uses an alert() or similar, the
// browser may continue to run a message loop and we will continue to receive
// messages, triggered by callbacks from the location providers, on the
// JavaScript thread before the callback returns.

#include "gears/geolocation/geolocation.h"

#include <limits>
#include <math.h>
#include "gears/base/common/js_runner.h"
#include "gears/base/common/permissions_manager.h"
#include "gears/base/common/stopwatch.h"  // For GetCurrentTimeMillis()
#include "gears/base/common/url_utils.h"  // For ResolveAndNormalize()
#include "gears/geolocation/location_provider_pool.h"
#include "gears/geolocation/device_data_provider.h"
#include "gears/geolocation/geolocation_db.h"

static const char16 *kDefaultLocationProviderUrl =
    STRING16(L"https://www.google.com/loc/json");

// API options constants.
static const char16 *kEnableHighAccuracy = STRING16(L"enableHighAccuracy");
static const char16 *kMaximumAge = STRING16(L"maximumAge");
static const char16 *kTimeout = STRING16(L"timeout");
static const char16 *kGearsRequestAddress = STRING16(L"gearsRequestAddress");
static const char16 *kGearsAddressLanguage = STRING16(L"gearsAddressLanguage");
static const char16 *kGearsLocationProviderUrls =
    STRING16(L"gearsLocationProviderUrls");

// Timing constants.
static const int64 kMinimumCallbackInterval = 1000;  // 1 second.
static const int64 kMaximumPositionFixAge = 60 * 1000;  // 1 minute.

// DB caching constants.
static const char16 *kLastPositionName = STRING16(L"LastPosition");

// MessageService constants.
static const char16 *kLocationAvailableObserverTopic =
    STRING16(L"location available");
static const char16 *kMovementDetectedObserverTopic =
    STRING16(L"movement detected");
static const char16 *kTimeoutExpiredObserverTopic =
    STRING16(L"timeout expired");
static const char16 *kCallbackRequiredObserverTopic =
    STRING16(L"callback required");

// Fix request constants.
static const int kLastRepeatingRequestId = kint32max;  // Repeating IDs positive
static const int kLastSingleRequestId = kint32min;  // Single IDs negative

// Position object constants.
static const char16 *kLatitudeString = STRING16(L"latitude");
static const char16 *kLongitudeString = STRING16(L"longitude");
static const char16 *kAccuracyString = STRING16(L"accuracy");
static const char16 *kAltitudeString = STRING16(L"altitude");
static const char16 *kAltitudeAccuracyString = STRING16(L"altitudeAccuracy");


TimedMessage::TimedMessage(int timeout_milliseconds,
                           const std::string16 &message_type,
                           NotificationData *notification_data)
    : message_type_(message_type) {
  assert(!message_type_.empty());
  assert(notification_data);
  timed_callback_.reset(
      new TimedCallback(this, timeout_milliseconds, notification_data));
  constructor_complete_event_.Signal();
}

// TimedCallback::ListenerInterface implementation
void TimedMessage::OnTimeout(TimedCallback *caller, void *user_data) {
  assert(caller);
  assert(user_data);

  // Wait for the constructor to complete so that we know that timed_callback_
  // has been assigned.
  constructor_complete_event_.Wait();
  assert(caller == timed_callback_.get());

  // We can't delete the TimedCallback here because we can't call Thread::Join()
  // from the worker thread. Instead we delete the TimedCallback when this
  // object is deleted in the message handler.
  NotificationData *notification_data =
      reinterpret_cast<NotificationData*>(user_data);
  MessageService::GetInstance()->NotifyObservers(message_type_.c_str(),
                                                 notification_data);
}


// Data classes for use with MessageService.
class NotificationDataGeoBase : public NotificationData {
 public:
  NotificationDataGeoBase(GearsGeolocation *object_in)
    : object(object_in) {}
  virtual ~NotificationDataGeoBase() {}

  friend class GearsGeolocation;

 private:
  // NotificationData implementation.
  //
  // We do not wish our messages to be delivered across process boundaries. To
  // achieve this, we use an unregistered class ID. On receipt of an IPC
  // message, InboundQueue::ReadOneMessage will attempt to deserialize the
  // message data using CreateAndReadObject. This will fail silently because no
  // factory function is registered for our class ID and Deserialize will not be
  // called.
  virtual SerializableClassId GetSerializableClassId() const {
    return SERIALIZABLE_GEOLOCATION;
  }
  virtual bool Serialize(Serializer * /* out */) const {
    // The serialized message is not used.
    return true;
  }
  virtual bool Deserialize(Deserializer * /* in */) {
    // This method should never be called.
    assert(false);
    return false;
  }

  GearsGeolocation *object;

  DISALLOW_EVIL_CONSTRUCTORS(NotificationDataGeoBase);
};

// Used for messages of types kLocationAvailableObserverTopic and
// kMovmementDetectedObserverTopic.
class ProviderNotificationData : public NotificationDataGeoBase {
 public:
  ProviderNotificationData(GearsGeolocation *object_in,
                           LocationProviderBase *provider_in)
      : NotificationDataGeoBase(object_in),
        provider(provider_in) {}
  virtual ~ProviderNotificationData() {}

 friend class GearsGeolocation;

 private:
  LocationProviderBase *provider;

  DISALLOW_EVIL_CONSTRUCTORS(ProviderNotificationData);
};

// Used for messages of types kTimeoutExpiredObserverTopic and
// kCallbackRequiredObserverTopic.
struct FixRequestIdNotificationData : public NotificationDataGeoBase {
 public:
  FixRequestIdNotificationData(GearsGeolocation *object_in,
                               int fix_request_id_in)
      : NotificationDataGeoBase(object_in),
        fix_request_id(fix_request_id_in) {}
  virtual ~FixRequestIdNotificationData() {}

  friend class GearsGeolocation;

 private:
  int fix_request_id;

  DISALLOW_EVIL_CONSTRUCTORS(FixRequestIdNotificationData);
};

// Helper function that checks if the caller had the required permissions
// to use this API. If the permissions are not set, it prompts the user.
// If the permissions cannot be acquired, it sets an exception and returns
// false. Else it returns true.
static bool AcquirePermissionForLocationData(ModuleImplBaseClass *geo_module,
                                             JsCallContext *context);

// Local functions

// Sets an object string property if the input value is valid.
static bool SetObjectPropertyIfValidString(const std::string16 &property_name,
                                           const std::string16 &value,
                                           JsObject *object);

// Returns true if there has been movement from the old position to the new
// position.
static bool IsNewPositionMovement(const Position &old_position,
                                  const Position &new_position);

// Returns true if the new position is more accurate than the old position.
static bool IsNewPositionMoreAccurate(const Position &old_position,
                                      const Position &new_position);

// Returns true if the old position is out of date.
static bool IsNewPositionMoreTimely(const Position &old_position,
                                    const Position &new_position);

// Helper function for CreateJavaScriptPositionObject. Creates a JavaScript
// address object, adding properties if they are specified. Returns true on
// success, even if no properties are added.
bool CreateJavaScriptAddressObject(const Address &address,
                                   JsObject *address_object);

DECLARE_DISPATCHER(GearsGeolocation);

template<>
void Dispatcher<GearsGeolocation>::Init() {
  RegisterProperty("lastPosition", &GearsGeolocation::GetLastPosition, NULL);
  RegisterProperty("hasPermission", &GearsGeolocation::GetHasPermission, NULL);
  RegisterMethod("getCurrentPosition", &GearsGeolocation::GetCurrentPosition);
  RegisterMethod("watchPosition", &GearsGeolocation::WatchPosition);
  RegisterMethod("clearWatch", &GearsGeolocation::ClearWatch);
  RegisterMethod("getPermission", &GearsGeolocation::GetPermission);
}

GearsGeolocation::GearsGeolocation()
    : ModuleImplBaseClass("GearsGeolocation"),
      next_single_request_id_(-1),
      next_watch_id_(1),
      unload_monitor_(NULL) {
  // Set up the thread message queue.
  if (!ThreadMessageQueue::GetInstance()->InitThreadMessageQueue()) {
    LOG(("Failed to set up thread message queue.\n"));
    assert(false);
    return;
  }

  MessageService *message_service = MessageService::GetInstance();
  assert(message_service);
  message_service->AddObserver(this, kLocationAvailableObserverTopic);
  message_service->AddObserver(this, kMovementDetectedObserverTopic);
  message_service->AddObserver(this, kTimeoutExpiredObserverTopic);
  message_service->AddObserver(this, kCallbackRequiredObserverTopic);

  // Retrieve the cached last known position, if available.
  GeolocationDB *db = GeolocationDB::GetDB();
  if (db) {
    // Only update last_position if the stored position is good. This should
    // always be the case.
    Position stored_position;
    if (db->RetrievePosition(kLastPositionName, &stored_position) &&
        stored_position.IsGoodFix()) {
      last_position_ = stored_position;
    }
  }
}

GearsGeolocation::~GearsGeolocation() {
  ASSERT_SINGLE_THREAD();

  // We should never be deleted until all pending requests have been cancelled.
#ifdef BROWSER_IEMOBILE
  // The lack of unload monitoring on IE Mobile on WinCE means that we may leak
  // providers and fix requests.
#else
  assert(providers_.empty());
  assert(fix_requests_.empty());
#endif  // BROWSER_IEMOBILE

  MessageService *message_service = MessageService::GetInstance();
  assert(message_service);
  message_service->RemoveObserver(this, kLocationAvailableObserverTopic);
  message_service->RemoveObserver(this, kMovementDetectedObserverTopic);
  message_service->RemoveObserver(this, kTimeoutExpiredObserverTopic);
  message_service->RemoveObserver(this, kCallbackRequiredObserverTopic);

  // Store the last known position.
  if (last_position_.IsGoodFix()) {
    GeolocationDB *db = GeolocationDB::GetDB();
    if (db) {
      db->StorePosition(kLastPositionName, last_position_);
    }
  }
}

// API Methods

void GearsGeolocation::GetLastPosition(JsCallContext *context) {
  ASSERT_SINGLE_THREAD();

  // Check permissions first.
  if (!AcquirePermissionForLocationData(this, context)) return;

  // If there's no good current position, we simply return null.
  if (!last_position_.IsGoodFix()) {
    return;
  }

  // Create the object for returning to JavaScript.
  scoped_ptr<JsObject> return_object(GetJsRunner()->NewObject());
  // If this method executes during page unload, the call to GetDispID
  // in JsRunnerBase::NewObjectWithArguments() can actually fail, so
  // we end up with a NULL object.
  if (!return_object.get()) {
    return;
  }

  if (!CreateJavaScriptPositionObject(last_position_,
                                      true,  // Use address if present.
                                      GetJsRunner(),
                                      return_object.get())) {
    LOG(("GearsGeolocation::GetLastPosition() : Failed to create position "
         "object.\n"));
    assert(false);
    return;
  }

  context->SetReturnValue(JSPARAM_OBJECT, return_object.get());
}

void GearsGeolocation::GetCurrentPosition(JsCallContext *context) {
  ASSERT_SINGLE_THREAD();

  // Check permissions first.
  if (!AcquirePermissionForLocationData(this, context)) return;

  GetPositionFix(context, false);
}

void GearsGeolocation::WatchPosition(JsCallContext *context) {
  ASSERT_SINGLE_THREAD();

  // Check permissions first.
  if (!AcquirePermissionForLocationData(this, context)) return;

  GetPositionFix(context, true);
}

void GearsGeolocation::ClearWatch(JsCallContext *context) {
  ASSERT_SINGLE_THREAD();

  // Check permissions first.
  if (!AcquirePermissionForLocationData(this, context)) return;

  int id;
  JsArgument argv[] = {
    { JSPARAM_REQUIRED, JSPARAM_INT, &id },
  };
  context->GetArguments(ARRAYSIZE(argv), argv);
  if (context->is_exception_set()) {
    return;
  }
  if (!CancelWatch(id)) {
    context->SetException(STRING16(L"Unknown watch ID ") +
                          IntegerToString16(id) +
                          STRING16(L"."));
  }
}

void GearsGeolocation::GetHasPermission(JsCallContext *context) {
  bool has_permission = GetPermissionsManager()->HasPermission(
      PermissionsDB::PERMISSION_LOCATION_DATA);
  context->SetReturnValue(JSPARAM_BOOL, &has_permission);
}

void GearsGeolocation::GetPermission(JsCallContext *context) {
  ASSERT_SINGLE_THREAD();

  scoped_ptr<PermissionsDialog::CustomContent> custom_content(
      PermissionsDialog::CreateCustomContent(context));
 
  if (!custom_content.get()) { return; }
 
  bool has_permission = GetPermissionsManager()->AcquirePermission(
      PermissionsDB::PERMISSION_LOCATION_DATA,
      EnvPageBrowsingContext(),
      custom_content.get());

  context->SetReturnValue(JSPARAM_BOOL, &has_permission);
}

// LocationProviderBase::ListenerInterface implementation.
bool GearsGeolocation::LocationUpdateAvailable(LocationProviderBase *provider) {
  assert(provider);

  // We check that the provider that invoked the callback is still in use, in
  // LocationUpdateAvailableImpl. Checking here would require a mutex to guard
  // providers_.

  // We marshall this callback onto the JavaScript thread. This simplifies
  // issuing new fix requests and calling back to JavaScript, which must be done
  // from the JavaScript thread.
  MessageService::GetInstance()->NotifyObservers(
      kLocationAvailableObserverTopic,
      new ProviderNotificationData(this, provider));
  return true;
}

bool GearsGeolocation::MovementDetected(LocationProviderBase *provider) {
  assert(provider);

  // We check that the provider that invoked the callback is still in use, in
  // MovementDetectedImpl. Checking here would require a mutex to guard
  // providers_.

  // We marshall this callback onto the JavaScript thread. This simplifies
  // issuing new fix requests and calling back to JavaScript, which must be done
  // from the JavaScript thread.
  MessageService::GetInstance()->NotifyObservers(
      kMovementDetectedObserverTopic,
      new ProviderNotificationData(this, provider));
  return true;
}

// MessageObserverInterface implementation.
void GearsGeolocation::OnNotify(MessageService *service,
                                const char16 *topic,
                                const NotificationData *data) {
  ASSERT_SINGLE_THREAD();
  assert(data);

  // Only respond to notifications made by this object.
  const NotificationDataGeoBase *geolocation_data =
      reinterpret_cast<const NotificationDataGeoBase*>(data);
  if (this != geolocation_data->object) {
    return;
  }

  std::string16 topic_string(topic);
  if (topic_string == kLocationAvailableObserverTopic) {
    // A location provider reported a new position.
    const ProviderNotificationData *location_available_data =
        reinterpret_cast<const ProviderNotificationData*>(data);
    LocationUpdateAvailableImpl(location_available_data->provider);
  } else if (topic_string == kMovementDetectedObserverTopic) {
    // A location provider reported movement.
    const ProviderNotificationData *location_available_data =
        reinterpret_cast<const ProviderNotificationData*>(data);
    MovementDetectedImpl(location_available_data->provider);
  } else if (topic_string == kTimeoutExpiredObserverTopic) {
    // The timeout for a new position being obtained expired.
    const FixRequestIdNotificationData *timeout_expired_data =
        reinterpret_cast<const FixRequestIdNotificationData*>(data);
    TimeoutExpiredImpl(timeout_expired_data->fix_request_id);
  } else if (topic_string == kCallbackRequiredObserverTopic) {
    // We need to make a callback.
    const FixRequestIdNotificationData *callback_required_data =
        reinterpret_cast<const FixRequestIdNotificationData*>(data);
    CallbackRequiredImpl(callback_required_data->fix_request_id);
  }
}

// JsEventHandlerInterface implementation.
void GearsGeolocation::HandleEvent(JsEventType event_type) {
  ASSERT_SINGLE_THREAD();
  assert(event_type == JSEVENT_UNLOAD);

  // Remove all fix requests. This cancels all pending requests by unregistering
  // from our location providers. Also delete the fix request objects and
  // decrement our ref count.
  //
  // We can't iterate over fix_requests_ directly, because RemoveFixRequest will
  // remove entries from the map and invalidate our iterator. Also, this object
  // may be GC'ed once we call DeleteFixRequest() on the last request, so we
  // can't use any member variables after this time.
  IdList fix_request_ids;
  for (FixRequestInfoMap::const_iterator iter = fix_requests_.begin();
       iter != fix_requests_.end();
       iter++) {
    fix_request_ids.push_back(iter->first);
  }
  for (int i = 0; i < static_cast<int>(fix_request_ids.size()); ++i) {
    RemoveAndDeleteFixRequest(fix_request_ids[i]);
  }
}

// Non-API methods

bool GearsGeolocation::IsCachedPositionSuitable(int maximum_age) {
  ASSERT_SINGLE_THREAD();

  if(maximum_age == 0) {
    return false;
  }
  if (!last_position_.IsGoodFix()) {
    return false;
  }
  if (maximum_age == -1) {  // Encoding for infinity.
    return true;
  }
  assert(maximum_age > 0);
  int64 age = GetCurrentTimeMillis() - last_position_.timestamp;
  assert(age >= 0);
  return age <= maximum_age;
}

void GearsGeolocation::AddProvidersToRequest(std::vector<std::string16> &urls,
                                             FixRequestInfo *info) {
  assert(info);

  // The lifetime of the providers is handled by the location provider pool,
  // through Register and Unregister.
  std::string16 host_name = EnvPageSecurityOrigin().host();
  LocationProviderPool *pool = LocationProviderPool::GetInstance();

  // Form the list of valid, absolute URLS.
  std::vector<std::string16> resolved_urls;
  for (int i = 0; i < static_cast<int>(urls.size()); ++i) {
    std::string16 resolved_url;
    if (ResolveAndNormalize(EnvPageLocationUrl().c_str(),
                            urls[i].c_str(),
                            &resolved_url)) {
      resolved_urls.push_back(resolved_url);
    }
  }

  // Mock provider
  LocationProviderBase *mock_provider =
      pool->Register(EnvPageBrowsingContext(),
                     STRING16(L"MOCK"),
                     STRING16(L""),  // url
                     STRING16(L""),  // host_name,
                     info->request_address,
                     STRING16(L""),  // info->address_language,
                     this);
  // This only succeeds in builds with CCTESTS when
  // ConfigureGeolocationMockLocationProviderForTest has been called.
  if (mock_provider) {
    info->providers.push_back(mock_provider);
  }

  // Native providers
  if (info->enable_high_accuracy) {
    // Use the first valid network provider URL, if present, for reverse
    // geocoding.
    std::string16 reverse_geocoder_url = resolved_urls.empty() ?
                                         STRING16(L"") :
                                         resolved_urls[0];
    LocationProviderBase *gps_provider =
        pool->Register(EnvPageBrowsingContext(),
                       STRING16(L"GPS"),
                       reverse_geocoder_url,
                       host_name,
                       info->request_address && !reverse_geocoder_url.empty(),
                       info->address_language,
                       this);
    // GPS is not available on all platforms.
    if (gps_provider) {
      info->providers.push_back(gps_provider);
    }
  }

  // Network providers
  for (int i = 0; i < static_cast<int>(resolved_urls.size()); ++i) {
    LocationProviderBase *network_provider =
        pool->Register(EnvPageBrowsingContext(),
                       STRING16(L"NETWORK"),
                       resolved_urls[i],
                       host_name,
                       info->request_address,
                       info->address_language,
                       this);
    // Network providers should always be available.
    assert(network_provider);
    info->providers.push_back(network_provider);
  }
}

void GearsGeolocation::GetPositionFix(JsCallContext *context, bool repeats) {
  ASSERT_SINGLE_THREAD();

  // The fact that Gears allows the caller to specify zero location providers
  // makes the logic around the use of a cached position with maximumAge a
  // little complex.
  // - maximumAge is zero. The cached position is not used. We throw an
  //   exception if no location providers are specified, or use the providers to
  //   obtain a new fix otherwise. (maximumAge defaults to zero, so this
  //   preserves the behaviour prior to the addition of maximumAge).
  // - maximumAge is non-zero and location providers are specified.
  //   - One-shot request. We call the success callback immediately if the
  //     cached position is suitable, or use the providers to obtain a new fix
  //     otherwise.
  //   - Repeating request. We call the success callback immediately if the
  //     cached position is suitable. In any case we then use the providers to
  //     obtain a new fix.
  // - maximumAge is non-zero and no location providers are specified. We call
  //   the success callback immediately if the cached position is suitable, or
  //   call the error callback immediately otherwise.

  // Get the arguments.
  std::vector<std::string16> urls;
  scoped_ptr<FixRequestInfo> info(new FixRequestInfo());
  if (!ParseArguments(context, &urls, info.get())) {
    assert(context->is_exception_set());
    return;
  }
  info->repeats = repeats;

  // Add the location providers. Note that we can't determine whether any
  // providers were specified in the request without actually adding them to the
  // request, and hence starting them. This is because ...
  // - Network provider URLs my be invalid
  // - The platform may not support GPS
  // - The mock provided may not be enabled
  // If timeout is zero, the providers will later be removed, but we have to add
  // them here so that we can test for the exception below. However, we can
  // optimise for the case when maximumAge is non-zero.
  if (!(info->timeout == 0 && info->maximum_age != 0)) {
    AddProvidersToRequest(urls, info.get());
  }

  // If this is a non-repeating request, hint to all providers that new data
  // is required ASAP.
  if (!info->repeats) {
    for (ProviderVector::iterator iter = info->providers.begin();
         iter != info->providers.end();
         ++iter) {
      (*iter)->UpdatePosition();
    }
  }

  // If this fix has maximumAge zero and no providers, throw an exception and
  // quit.
  if (info->maximum_age == 0 && info->providers.empty()) {
    context->SetException(STRING16(L"Fix request has no location providers."));
    return;
  }

  // See whether we need to call back immediately with a cached position or a
  // cached position error.
  Position position_to_call_back;
  if (IsCachedPositionSuitable(info->maximum_age)) {
    position_to_call_back = last_position_;
  } else if (info->providers.empty() && info->timeout != 0) {
    position_to_call_back.error_code =
        Position::ERROR_CODE_POSITION_UNAVAILABLE;
    position_to_call_back.error_message =
        STRING16(L"No suitable cached position available.");
  }

  // Record this fix. This updates the map of providers for this fix request
  // and provides a fix request ID. The map takes ownership of the info
  // structure on success.
  int fix_request_id = 0;
  if (!RecordNewFixRequest(info.get(), &fix_request_id)) {
    context->SetException(STRING16(L"Exceeded maximum number of fix "
                                   L"requests."));
    return;
  }
  assert(fix_request_id != 0);

  // A timeout of zero is a special case which means that we never use the
  // providers to obtain a position fix. In this case, we can and must cancel
  // them.
  if (info->timeout == 0) {
    RemoveAllProviders(fix_request_id);
  }

  // Invoke the callback for the cached position or error. We use a message to
  // do so asynchronously. Note that the callback handler cancels any active
  // providers.
  if (position_to_call_back.IsInitialized()) {
    assert(!info->success_callback_timer.get());

    info->pending_position = position_to_call_back;
    info->success_callback_timer.reset(new TimedMessage(
        0,  // Call back immediately
        kCallbackRequiredObserverTopic,
        new FixRequestIdNotificationData(this, fix_request_id)));
  }

  // Start the timeout timer for this fix request. If the timeout is zero and we
  // have a cached position or cached position error, we want to callback with
  // the cached position, not the timeout. However, there is a potential race
  // condition between this callback and the timer callback because callbacks
  // are invoked asynchronously and there are no guarantees on ordering. So in
  // this case, we do not start the timeout timer.
  if (!(info->timeout == 0 && position_to_call_back.IsInitialized())) {
    StartTimeoutTimer(fix_request_id);
  }

  // Set the return value if this fix repeats.
  if (info->repeats) {
    context->SetReturnValue(JSPARAM_INT, &fix_request_id);
  }

  // The map has ownership of this fix request.
  info.release();
}

bool GearsGeolocation::CancelWatch(const int &watch_id) {
  ASSERT_SINGLE_THREAD();

  FixRequestInfo *info = MaybeGetFixRequest(watch_id);
  if (info == NULL) {
    return false;
  }
  if (!info->repeats) {
    return false;
  }
  assert(watch_id > 0);

  // Update the map of providers that this fix request has been deleted.
  RemoveAndDeleteFixRequest(watch_id);

  return true;
}

void GearsGeolocation::HandleRepeatingRequestUpdate(int fix_request_id,
                                                    const Position &position) {
  ASSERT_SINGLE_THREAD();
  assert(position.IsInitialized());

  FixRequestInfo *fix_info = GetFixRequest(fix_request_id);
  assert(fix_info->repeats);

  // If this is an error, make a callback.
  if (!position.IsGoodFix()) {
    MakeErrorCallback(fix_info, position);
    return;
  }

  // This is a position update. If there's currently a timer running for a
  // movement timeout, stop it.
  fix_info->timeout_timer.reset();

  if (IsNewPositionMovement(fix_info->last_position, position) ||
      IsNewPositionMoreAccurate(fix_info->last_position, position)) {
    // The position has changed significantly. See if there's currently a
    // success callback pending. If so, simply update the position it will use,
    // if applicable.
    if (fix_info->success_callback_timer.get()) {
      if (IsNewPositionMovement(fix_info->pending_position, position) ||
          IsNewPositionMoreAccurate(fix_info->pending_position, position)) {
        fix_info->pending_position = position;
      }
      return;
    }

    // See if the minimum time interval since the last callback has expired.
    int64 time_remaining =
        kMinimumCallbackInterval -
        (GetCurrentTimeMillis() - fix_info->last_success_callback_time);
    if (time_remaining <= 0) {
      if (!MakeSuccessCallback(fix_info, position)) {
        LOG(("GearsGeolocation::HandleRepeatingRequestUpdate() : JavaScript "
             "callback failed.\n"));
      }
    } else {
      // Start an asynchronous timer which will post a message back to this
      // thread once the minimum time period has elapsed.
      MakeFutureWatchSuccessCallback(static_cast<int>(time_remaining),
                                     fix_request_id,
                                     position);
    }
  }
}

void GearsGeolocation::StartTimeoutTimer(int fix_request_id) {
  ASSERT_SINGLE_THREAD();

  // If this request already has a timer running, we don't update the timer
  // because we still require a position fix within the timeout period from the
  // first detection of movement.
  FixRequestInfo *fix_request_info = GetFixRequest(fix_request_id);
  if (fix_request_info->timeout_timer.get()) {
    return;
  }

  // If this fix request has a timeout specified, set a timer for the specified
  // interval.
  if (fix_request_info->timeout >= 0) {
    fix_request_info->timeout_timer.reset(new TimedMessage(
        fix_request_info->timeout,
        kTimeoutExpiredObserverTopic,
        new FixRequestIdNotificationData(this, fix_request_id)));
  }
}

void GearsGeolocation::MakeTimeoutExpiredCallback(int fix_request_id) {
  ASSERT_SINGLE_THREAD();

  // Confirm that this request uses a timeout.
  FixRequestInfo *fix_request_info = GetFixRequest(fix_request_id);
  assert(fix_request_info->timeout >= 0);

  // Ref this object to avoid it being deleted before this method completes, in
  // the case where this callback removes our final reference.
  Ref();

  // See if we are a one-shot:
  bool is_one_shot = !fix_request_info->repeats;

  // If this is a one-shot request, remove the fix request from our map, so that
  // position updates which occur while the callback to JavaScript is in process
  // do not trigger handling for this fix request.
  if (is_one_shot) {
    assert(fix_request_id < 0);
    RemoveFixRequest(fix_request_id);
  }

  // Call back to JavaScript with an error.
  Position position;
  position.error_code = Position::ERROR_CODE_TIMEOUT;
  position.error_message = STRING16(L"A position fix was not obtained within "
                                    L"the specified time limit.");
  if (!MakeErrorCallback(fix_request_info, position)) {
    LOG(("GearsGeolocation::MakeTimeoutExpiredCallback() : JavaScript error "
         "callback failed.\n"));
  }

#ifdef DEBUG
  // Force garbage collection to check that ref counts work as intended.
  GetJsRunner()->ForceGC();
#endif

  if (is_one_shot) {
    // If it's a non-repeating request, remove the fix request.  Note
    // that this one-shot fix request could not have been removed in
    // the callback since there is no explict API that would allow an
    // application to do so.
    DeleteFixRequest(fix_request_info);
  }

  Unref();
}

void GearsGeolocation::HandleSingleRequestUpdate(LocationProviderBase *provider,
                                                 int fix_request_id,
                                                 const Position &position) {
  ASSERT_SINGLE_THREAD();
  assert(position.IsInitialized());

  FixRequestInfo *fix_info = GetFixRequest(fix_request_id);
  assert(!fix_info->repeats);
  assert(fix_info->last_success_callback_time == 0);

  // Remove this provider from the this fix so that future callbacks to this
  // Geolocation object don't trigger handling for this fix.
  RemoveProvider(provider, fix_request_id);
  // We callback in two cases ...
  // - This response gives a good position and we haven't yet called back
  // - The fix has no remaining providers, so we'll never get a valid position
  // We then cancel any pending requests and delete the fix request.
  if (position.IsGoodFix() || fix_info->providers.empty()) {
    // This is a good fix (or a failure from the last provider). If there's
    // currently a timer running for a movement timeout, stop it.
    fix_info->timeout_timer.reset();

    // Remove the fix request from our map, so that position updates which occur
    // while the callback to JavaScript is in process do not trigger handling
    // for this fix request.
    RemoveFixRequest(fix_request_id);
    if (position.IsGoodFix()) {
      if (!MakeSuccessCallback(fix_info, position)) {
        LOG(("GearsGeolocation::HandleSingleRequestUpdate() : JavaScript "
             "success callback failed.\n"));
      }
    } else {
      if (!MakeErrorCallback(fix_info, position)) {
        LOG(("GearsGeolocation::HandleSingleRequestUpdate() : JavaScript error "
             "callback failed.\n"));
      }
    }
    DeleteFixRequest(fix_info);
  }
}

void GearsGeolocation::LocationUpdateAvailableImpl(
    LocationProviderBase *provider) {
  ASSERT_SINGLE_THREAD();

  // Check that the provider that invoked the callback is still in use. By the
  // time we receive this marshalled callback the provider may have been
  // unregistered and deleted.
  ProviderMap::iterator provider_iter = providers_.find(provider);
  if (provider_iter == providers_.end()) {
    return;
  }

  // Get the position from the provider.
  Position position;
  provider->GetPosition(&position);
  
  // The location provider should only call us back when it has a valid
  // position. However, it's possible that this marshalled callback is due to a
  // previous provider, which no longer exists. If a new provider is now
  // registered at the same address as the previous one, the above check will
  // not detect this case and we'll call GetPosition on the new provider without
  // it having previously called us back.
  if (!position.IsInitialized()) {
    return;
  }

  // Update the last known position, which is the best position estimate we
  // currently have.
  // TODO(steveblock): Rather than updating when there is significant movement,
  // we should probably update (though not neccessarily call back) if there is
  // any movmement, provided the accuracy is not worse.
  if (IsNewPositionMovement(last_position_, position) ||
      IsNewPositionMoreAccurate(last_position_, position) ||
      IsNewPositionMoreTimely(last_position_, position)) {
    last_position_ = position;
  }

  // Whenever a fix request makes its last callback, we unref this object. This
  // means that once the last fix request has made its last callback, we risk
  // this object being deleted before this method completes. We therefore ref
  // and unref this object around the callback and unref calls.
  Ref();

  // The HandleXXXRequestUpdate methods called below may make a callback to
  // JavaScript. A callback may ...
  // - call back into Gears code and remove a repeating fix request.
  // - call alert() or equivalent, which allows this method to be called
  //   again in repsonse to updates from providers. This could result in fix
  //   requests of either type being removed from the map.
  // For these reasons, we can't iterate on a list of fix requests directly.
  // Instead, we first take a copy of the fix request IDs. Then, at each
  // iteration, we check to see if that ID is still valid.

  // Iterate over all non-repeating fix requests of which this provider is a
  // part.
  assert(provider_iter != providers_.end());
  IdList ids = provider_iter->second;
  while (!ids.empty()) {
    int id = ids.back();
    ids.pop_back();
    if (id < 0) {
      if (MaybeGetFixRequest(id)) {
        HandleSingleRequestUpdate(provider, id, position);
      }
    }
  }

#ifdef DEBUG
  // Force garbage collection to guard against regression. See comment above.
  GetJsRunner()->ForceGC();
#endif

  // Iterate over all repeating fix requests.
  IdList watch_ids;
  for (FixRequestInfoMap::const_iterator iter = fix_requests_.begin();
       iter != fix_requests_.end();
       iter++) {
    int id = iter->first;
    if (id > 0) {
      watch_ids.push_back(id);
    }
  }
  while (!watch_ids.empty()) {
    int watch_id = watch_ids.back();
    watch_ids.pop_back();
    if (MaybeGetFixRequest(watch_id)) {
      HandleRepeatingRequestUpdate(watch_id, position);
    }
  }

  Unref();
}

void GearsGeolocation::MovementDetectedImpl(LocationProviderBase *provider) {
  ASSERT_SINGLE_THREAD();

  // Check that we're still using this provider.
  ProviderMap::iterator provider_iter = providers_.find(provider);
  if (provider_iter == providers_.end()) {
    return;
  }

  // This provider has detected movement so is in the process of obtaining a new
  // fix. Go through all of the fix requests of which this provider is a part
  // and register this fact. This is to allow the arbitrator to apply the
  // timeout constraint.
  //
  // See comment in LocationUpdateAvailableImpl for why we take a copy of the
  // fix request IDs.
  IdList fix_request_ids = provider_iter->second;
  while (!fix_request_ids.empty()) {
    int id = fix_request_ids.back();
    fix_request_ids.pop_back();
    FixRequestInfoMap::const_iterator iter = fix_requests_.find(id);
    if (iter != fix_requests_.end()) {
      // Start the timeout timer for this fix request. We don't care which
      // provider detected the movement.
      StartTimeoutTimer(id);
    }
  }
}

void GearsGeolocation::TimeoutExpiredImpl(int fix_request_id) {
  ASSERT_SINGLE_THREAD();

  // Check that the fix request still exists and that the timeout timer has not
  // been cancelled.
  FixRequestInfo *fix_info = MaybeGetFixRequest(fix_request_id);
  if (fix_info && fix_info->timeout_timer.get()) {
    fix_info->timeout_timer.reset();
    MakeTimeoutExpiredCallback(fix_request_id);
  }
}

void GearsGeolocation::CallbackRequiredImpl(int fix_request_id) {
  ASSERT_SINGLE_THREAD();

  // Check that the fix request still exists.
  FixRequestInfo *fix_info = MaybeGetFixRequest(fix_request_id);
  if (!fix_info) {
    return;
  }

  if (!fix_info->repeats) {
    // Remove the fix request from our map, so that position updates which occur
    // while the callback to JavaScript is in process do not trigger handling
    // for this fix request.
    RemoveFixRequest(fix_request_id);
  }

  assert(fix_info->success_callback_timer.get());
  fix_info->success_callback_timer.reset();

  assert(fix_info->pending_position.IsInitialized());
  if (fix_info->pending_position.IsGoodFix()) {
    if (!MakeSuccessCallback(fix_info, fix_info->pending_position)) {
      LOG(("GearsGeolocation::CallbackRequiredImpl() : JavaScript "
           "success callback failed.\n"));
    }
  } else {
    if (!MakeErrorCallback(fix_info, fix_info->pending_position)) {
      LOG(("GearsGeolocation::CallbackRequiredImpl() : JavaScript "
           "error callback failed.\n"));
    }
  }

  if (!fix_info->repeats) {
    DeleteFixRequest(fix_info);
  }
}

bool GearsGeolocation::MakeSuccessCallback(FixRequestInfo *fix_info,
                                           const Position &position) {
  ASSERT_SINGLE_THREAD();
  assert(fix_info);
  assert(position.IsInitialized());
  assert(position.IsGoodFix());

  scoped_ptr<JsObject> position_object(GetJsRunner()->NewObject());
  // If this method executes during page unload, the call to GetDispID
  // in JsRunnerBase::NewObjectWithArguments() can actually fail, so
  // we end up with a NULL object.
  if (!position_object.get()) {
    return false;
  }

  if (!CreateJavaScriptPositionObject(position,
                                      fix_info->request_address,
                                      GetJsRunner(),
                                      position_object.get())) {
    LOG(("GearsGeolocation::MakeSuccessCallback() : Failed to create position "
         "object.\n"));
    assert(false);
    return false;
  }
  fix_info->last_position = position;
  fix_info->last_success_callback_time = GetCurrentTimeMillis();
  JsParamToSend argv[] = { { JSPARAM_OBJECT, position_object.get() } };

  // InvokeCallback returns false if the callback enounters an error. Once we've
  // made the callback, we can't rely on any of the fix request data, because it
  // could be removed by other calls to this object before the callback returns.
  GetJsRunner()->InvokeCallback(fix_info->success_callback.get(), NULL,
                                ARRAYSIZE(argv), argv, NULL);
  return true;
}

bool GearsGeolocation::MakeErrorCallback(FixRequestInfo *fix_info,
                                         const Position &position) {
  ASSERT_SINGLE_THREAD();
  assert(fix_info);
  assert(position.IsInitialized());
  assert(!position.IsGoodFix());

  // The error callback is optional.
  if (!fix_info->error_callback.get()) {
    return true;
  }

  scoped_ptr<JsObject> position_error_object(GetJsRunner()->NewObject());
  // If this method executes during page unload, the call to GetDispID
  // in JsRunnerBase::NewObjectWithArguments() can actually fail, so
  // we end up with a NULL object.
  if (!position_error_object.get()) {
    return false;
  }

  if (!CreateJavaScriptPositionErrorObject(position,
                                           position_error_object.get())) {
    LOG(("GearsGeolocation::MakeErrorCallback() : Failed to create position "
         "error object.\n"));
    assert(false);
    return false;
  }
  JsParamToSend argv[] = { { JSPARAM_OBJECT, position_error_object.get() } };

  // InvokeCallback returns false if the callback enounters an error. Once we've
  // made the callback, we can't rely on any of the fix request data, because it
  // could be removed by other calls to this object before the callback returns.
  GetJsRunner()->InvokeCallback(fix_info->error_callback.get(), NULL,
                                ARRAYSIZE(argv), argv, NULL);
  return true;
}

// static
bool GearsGeolocation::ParseArguments(JsCallContext *context,
                                      std::vector<std::string16> *urls,
                                      FixRequestInfo *info) {
  assert(context);
  assert(urls);
  assert(info);

  // Arguments are: function successCallback, optional function errorCallback,
  // optional object options. errorCallback can be null.
  //
  // Note that GetArguments allocates a new JsRootedCallback.
  //
  JsRootedCallback *success_callback = NULL;
  JsRootedCallback *error_callback = NULL;
  scoped_ptr<JsObject> options;
  JsArgument argv[] = {
    { JSPARAM_REQUIRED, JSPARAM_FUNCTION, &success_callback, false },
    { JSPARAM_OPTIONAL, JSPARAM_FUNCTION, &error_callback, false },
    { JSPARAM_OPTIONAL, JSPARAM_OBJECT, as_out_parameter(options), false },
  };
 
  bool success = context->GetArguments(ARRAYSIZE(argv), argv);
  if (!success) {
    delete success_callback;
    delete error_callback;
    return false;
  }

  // Set the success callback
  assert(success_callback);
  info->success_callback.reset(success_callback);

  // Set the error callback, using NULL if it was not specified.
  if (argv[1].was_specified) {
    info->error_callback.reset(error_callback);
  }

  // Set default values for options.
  info->enable_high_accuracy = false;
  info->maximum_age = 0;
  info->timeout = -1;  // No limit is applied
  info->request_address = false;
  urls->clear();
  // We have to check that options is present because it's not valid to use an
  // uninitialised JsObject.
  if (argv[2].was_specified) {
    if (!ParseOptions(context, options.get(), urls, info)) {
      assert(context->is_exception_set());
      return false;
    }
  } else {
    // options is not specified, so use the default URL.
    urls->push_back(kDefaultLocationProviderUrl);
  }
  return true;
}

// static
bool GearsGeolocation::ParseOptions(JsCallContext *context,
                                    const JsObject *options,
                                    std::vector<std::string16> *urls,
                                    FixRequestInfo *info) {
  assert(context);
  assert(urls);
  assert(info);
  if (options->GetPropertyType(kEnableHighAccuracy) != JSPARAM_UNDEFINED &&
      !options->GetPropertyAsBool(kEnableHighAccuracy,
                                  &(info->enable_high_accuracy))) {
    std::string16 error = STRING16(L"options.");
    error += kEnableHighAccuracy;
    error += STRING16(L" should be a boolean.");
    context->SetException(error);
    return false;
  }
  if (options->GetPropertyType(kMaximumAge) != JSPARAM_UNDEFINED) {
    double maximum_age_double;
    int maximum_age_int;
    // Floating point types should support infinity.
    assert(std::numeric_limits<double>::has_infinity);
    if (std::numeric_limits<double>::has_infinity &&
        options->GetPropertyAsDouble(kMaximumAge, &maximum_age_double) &&
        maximum_age_double == std::numeric_limits<double>::infinity()) {
      info->maximum_age = -1;
    } else if (options->GetPropertyAsInt(kMaximumAge, &maximum_age_int) &&
        maximum_age_int >= 0) {
      info->maximum_age = maximum_age_int;
    } else {
      std::string16 error = STRING16(L"options.");
      error += kMaximumAge;
      error += STRING16(L" should be a non-negative 32 bit signed integer or "
                        L"Infinity.");
      context->SetException(error);
      return false;
    }
    assert(info->maximum_age >= -1);
  }
  if (options->GetPropertyType(kTimeout) != JSPARAM_UNDEFINED) {
    int timeout = -1;
    if (!options->GetPropertyAsInt(kTimeout, &timeout) ||
        timeout < 0) {
      std::string16 error = STRING16(L"options.");
      error += kTimeout;
      error += STRING16(L" should be a non-negative 32 bit signed integer.");
      context->SetException(error);
      return false;
    }
    info->timeout = timeout;
    assert(info->timeout >= 0);
  }
  if (options->GetPropertyType(kGearsRequestAddress) != JSPARAM_UNDEFINED &&
      !options->GetPropertyAsBool(kGearsRequestAddress,
                                  &(info->request_address))) {
    std::string16 error = STRING16(L"options.");
    error += kGearsRequestAddress;
    error += STRING16(L" should be a boolean.");
    context->SetException(error);
    return false;
  }
  if (options->GetPropertyType(kGearsAddressLanguage) != JSPARAM_UNDEFINED) {
    std::string16 address_language;
    if (!options->GetPropertyAsString(kGearsAddressLanguage,
                                      &address_language) ||
        address_language.empty()) {
      std::string16 error = STRING16(L"options.");
      error += kGearsAddressLanguage;
      error += STRING16(L" should be a non-empty string.");
      context->SetException(error);
      return false;
    }
    info->address_language = address_language;
    assert(!info->address_language.empty());
  }
  if (options->GetPropertyType(kGearsLocationProviderUrls) !=
          JSPARAM_UNDEFINED) {
    JsParamType type = options->GetPropertyType(kGearsLocationProviderUrls);
    // If gearsLocationProviderUrls is null, we do not use the default URL.
    if (type != JSPARAM_NULL) {
      scoped_ptr<JsArray> js_array;
      if (!options->GetPropertyAsArray(kGearsLocationProviderUrls,
                                      as_out_parameter(js_array)) ||
          !ParseLocationProviderUrls(context, js_array.get(), urls)) {
        // If it's not an array and not null, this is an error.
        std::string16 error = STRING16(L"options.");
        error += kGearsLocationProviderUrls;
        error += STRING16(L" should be null or an array of strings.");
        context->SetException(error);
        return false;
      }
    }
  } else {
    // gearsLocationProviderUrls is not specified, so use the default URL.
    urls->push_back(kDefaultLocationProviderUrl);
  }
  return true;
}

// static
bool GearsGeolocation::ParseLocationProviderUrls(
    JsCallContext *context,
    const JsArray *js_array,
    std::vector<std::string16> *urls) {
  assert(context);
  assert(urls);
  int length;
  if (!js_array->GetLength(&length)) {
    LOG(("GearsGeolocation::ParseLocationProviderUrls() : Failed to get "
          "length of gearsLocationProviderUrls."));
    assert(false);
    return false;
  }
  for (int i = 0; i < length; ++i) {
    std::string16 url;
    if (!js_array->GetElementAsString(i, &url)) {
      return false;
    }
    urls->push_back(url);
  }
  return true;
}

// static
bool GearsGeolocation::CreateJavaScriptPositionObject(
    const Position &position,
    bool use_address,
    JsRunnerInterface *js_runner,
    JsObject *position_object) {
  assert(js_runner);
  assert(position_object);
  assert(position.IsInitialized());
  assert(position.IsGoodFix());

  bool result = true;

  // The timestamp is a property of the position object. It should always be
  // valid.
  scoped_ptr<JsObject> date_object(js_runner->NewDate(position.timestamp));
  if (!date_object.get()) {
    assert(false);
    return false;
  }
  result &= position_object->SetPropertyObject(STRING16(L"timestamp"),
                                               date_object.get());

  // The W3C spec stipulates that the latitude, longitude etc should be members
  // of the coords object. In addition, we add these properties to the position
  // object directly to maintain backwards compatibility with the pre-exisiting
  // Gears API.
  scoped_ptr<JsObject> coords_object(js_runner->NewObject());
  if (!coords_object.get()) {
    assert(false);
    return false;
  }

  // latitude, longitude and accuracy should always be valid.
  result &= position_object->SetPropertyDouble(kLatitudeString,
                                               position.latitude);
  result &= position_object->SetPropertyDouble(kLongitudeString,
                                               position.longitude);
  result &= position_object->SetPropertyDouble(kAccuracyString,
                                               position.accuracy);
  result &= coords_object->SetPropertyDouble(kLatitudeString,
                                             position.latitude);
  result &= coords_object->SetPropertyDouble(kLongitudeString,
                                             position.longitude);
  result &= coords_object->SetPropertyDouble(kAccuracyString,
                                             position.accuracy);

  // Other properties may not be valid.
  if (position.altitude > kBadAltitude) {
    result &= position_object->SetPropertyDouble(kAltitudeString,
                                                 position.altitude);
    result &= coords_object->SetPropertyDouble(kAltitudeString,
                                               position.altitude);
  }
  if (position.altitude_accuracy >= 0.0) {
    result &= position_object->SetPropertyDouble(kAltitudeAccuracyString,
                                                 position.altitude_accuracy);
    result &= coords_object->SetPropertyDouble(kAltitudeAccuracyString,
                                               position.altitude_accuracy);
  }

  result &= position_object->SetPropertyObject(STRING16(L"coords"),
                                               coords_object.get());

  // Address
  if (use_address) {
    scoped_ptr<JsObject> address_object(js_runner->NewObject());
    if (!address_object.get()) {
      assert(false);
      return false;
    }
    result &= CreateJavaScriptAddressObject(position.address,
                                            address_object.get());
    // Only add the address object if it has some properties.
    std::vector<std::string16> properties;
    if (address_object.get()->GetPropertyNames(&properties) &&
        !properties.empty()) {
      result &= position_object->SetPropertyObject(STRING16(L"gearsAddress"),
                                                   address_object.get());
    }
  }

  return result;
}

// static
bool GearsGeolocation::CreateJavaScriptPositionErrorObject(
    const Position &position,
    JsObject *error) {
  assert(error);
  assert(position.IsInitialized());
  assert(!position.IsGoodFix());

  bool result = true;
  // We provide the values of all W3C error constants.
  result &= error->SetPropertyInt(STRING16(L"UNKNOWN_ERROR"),
                                  Position::ERROR_CODE_UNKNOWN_ERROR);
  result &= error->SetPropertyInt(STRING16(L"PERMISSION_DENIED"),
                                  Position::ERROR_CODE_PERMISSION_DENIED);
  result &= error->SetPropertyInt(STRING16(L"POSITION_UNAVAILABLE"),
                                  Position::ERROR_CODE_POSITION_UNAVAILABLE);
  result &= error->SetPropertyInt(STRING16(L"TIMEOUT"),
                                  Position::ERROR_CODE_TIMEOUT);
  // error_code should always be valid.
  result &= error->SetPropertyInt(STRING16(L"code"),
                                  position.error_code);
  // Other properties may not be valid.
  result &= SetObjectPropertyIfValidString(STRING16(L"message"),
                                           position.error_message,
                                           error);
  return result;
}

GearsGeolocation::FixRequestInfo *GearsGeolocation::GetFixRequest(int id) {
  ASSERT_SINGLE_THREAD();

  FixRequestInfo *fix_info = MaybeGetFixRequest(id);
  assert(fix_info);
  return fix_info;
}

GearsGeolocation::FixRequestInfo *GearsGeolocation::MaybeGetFixRequest(int id) {
  ASSERT_SINGLE_THREAD();

  FixRequestInfoMap::const_iterator iter = fix_requests_.find(id);
  return iter == fix_requests_.end() ? NULL : iter->second;
}

bool GearsGeolocation::RecordNewFixRequest(FixRequestInfo *fix_request,
                                           int *fix_request_id) {
  ASSERT_SINGLE_THREAD();
  assert(fix_request_id);

  if (fix_request->repeats) {
    if (next_watch_id_ == kLastRepeatingRequestId) {
      return false;
    }
    *fix_request_id = next_watch_id_++;
  } else {
    if (next_watch_id_ == kLastSingleRequestId) {
      return false;
    }
    *fix_request_id = next_single_request_id_--;
  }
  fix_requests_[*fix_request_id] = fix_request;

  // For each location provider used by this request, update the provider's
  // list of fix requests in the map.
  ProviderVector *member_providers = &fix_request->providers;
  for (ProviderVector::iterator iter = member_providers->begin();
       iter != member_providers->end();
       ++iter) {
    LocationProviderBase *provider = *iter;
    // If providers_ does not yet have an entry for this provider, this will
    // create one.
    providers_[provider].push_back(*fix_request_id);
  }

  // Increment our ref count to keep this object in scope until we make the
  // callback or cancel the request.
  Ref();

  // Make sure we have an unload monitor in place to cancel pending requests
  // when the page unloads.
  if (unload_monitor_ == NULL) {
    unload_monitor_.reset(new JsEventMonitor(GetJsRunner(), JSEVENT_UNLOAD,
                                             this));
  }

  return true;
}

void GearsGeolocation::RemoveFixRequest(int fix_request_id) {
  ASSERT_SINGLE_THREAD();

  RemoveAllProviders(fix_request_id);
  fix_requests_.erase(fix_request_id);
}

void GearsGeolocation::RemoveAllProviders(int fix_request_id) {
  ASSERT_SINGLE_THREAD();

  FixRequestInfo *fix_request = GetFixRequest(fix_request_id);

  // For each location provider used by this request, update the provider's
  // list of fix requests in the map.
  ProviderVector *member_providers = &fix_request->providers;
  while (!member_providers->empty()) {
    LocationProviderBase *provider = member_providers->back();
    member_providers->pop_back();
    assert(provider);

    // Check that we have an entry in the map for this provider.
    ProviderMap::iterator provider_iter = providers_.find(provider);
    assert(provider_iter != providers_.end());

    // Find this fix request in the list of fix requests for this provider.
    IdList *ids = &(provider_iter->second);
    IdList::iterator id_iterator =
        std::find(ids->begin(), ids->end(), fix_request_id);

    // If we can't find this request in the list, something has gone wrong.
    assert(id_iterator != ids->end());

    // Remove this fix request from the list of fix requests for this provider.
    ids->erase(id_iterator);

    // If this location provider is no longer used in any fixes, remove it from
    // our map.
    if (ids->empty()) {
      providers_.erase(provider_iter);
    }

    // Unregister from the provider, via the pool. If there are no more
    // listeners for this provider, this will cancel any pending requests and
    // may block if a callback is currently in progress. The pool takes care of
    // ref counting for the multiple fix requests that use this provider, even
    // for the same listener.
    LocationProviderPool::GetInstance()->Unregister(provider, this);
  }
}

void GearsGeolocation::DeleteFixRequest(FixRequestInfo *fix_request) {
  ASSERT_SINGLE_THREAD();

  delete fix_request;
  // Decrement the ref count since we will no longer call back for this fix
  // request.
  Unref();
}

void GearsGeolocation::RemoveAndDeleteFixRequest(int fix_request_id) {
  ASSERT_SINGLE_THREAD();

  // Cache the pointer to the fix request because RemoveFixRequest will
  // invalidate the iterator.
  FixRequestInfo *fix_request = GetFixRequest(fix_request_id);
  RemoveFixRequest(fix_request_id);
  DeleteFixRequest(fix_request);
}

void GearsGeolocation::RemoveProvider(LocationProviderBase *provider,
                                      int fix_request_id) {
  ASSERT_SINGLE_THREAD();

  FixRequestInfo *fix_request = GetFixRequest(fix_request_id);
  assert(!fix_request->repeats);

  ProviderVector *member_providers = &fix_request->providers;
  ProviderVector::iterator iter = std::find(member_providers->begin(),
                                            member_providers->end(),
                                            provider);
  // Check that this provider is used by the fix request.
  assert(iter != member_providers->end());

  // Remove the location provider from the fix request.
  member_providers->erase(iter);

  // Remove this fix request from the provider in the map of providers.
  ProviderMap::iterator provider_iter = providers_.find(provider);
  assert(provider_iter != providers_.end());
  IdList *ids = &(provider_iter->second);
  IdList::iterator id_iterator =
      std::find(ids->begin(), ids->end(), fix_request_id);
  assert(id_iterator != ids->end());
  ids->erase(id_iterator);

  // If this location provider is no longer used in any fixes, remove it from
  // our map.
  if (ids->empty()) {
    providers_.erase(provider_iter);
  }

  // Unregister from the provider, via the pool. If there are no more
  // listeners for this provider, this will cancel any pending requests and
  // may block if a callback is currently in progress. The pool takes care of
  // ref counting for the multiple fix requests that use this provider, even
  // for the same listener.
  LocationProviderPool::GetInstance()->Unregister(provider, this);
}

void GearsGeolocation::MakeFutureWatchSuccessCallback(
    int timeout_milliseconds,
    int fix_request_id,
    const Position &position) {
  ASSERT_SINGLE_THREAD();

  // Check that there isn't already a timer running for this request.
  FixRequestInfo *fix_info = GetFixRequest(fix_request_id);
  assert(!fix_info->success_callback_timer.get());
  assert(fix_info->repeats);

  fix_info->pending_position = position;
  fix_info->success_callback_timer.reset(new TimedMessage(
      timeout_milliseconds,
      kCallbackRequiredObserverTopic,
      new FixRequestIdNotificationData(this, fix_request_id)));
}

// Local functions

static bool SetObjectPropertyIfValidString(const std::string16 &property_name,
                                           const std::string16 &value,
                                           JsObject *object) {
  assert(object);
  if (!value.empty()) {
    return object->SetPropertyString(property_name, value);
  }
  return true;
}

// Helper function for IsMovement, IsMoreAccurate and IsNewPositionMoreTimely.
// Checks whether the old or new position is bad, in which case the decision of
// which to use is easy. If this case is detected, the return value is true and
// result indicates whether the new position should be used.
static bool CheckForBadPosition(const Position &old_position,
                                const Position &new_position,
                                bool *result) {
  assert(result);
  if (!new_position.IsGoodFix()) {
    // New is bad.
    *result = false;
    return true;
  } if (!old_position.IsGoodFix()) {
    // Old is bad, new is good.
    *result = true;
    return true;
  }
  return false;
}

// Returns true if there has been movement from the old position to the new
// position.
static bool IsNewPositionMovement(const Position &old_position,
                                  const Position &new_position) {
  bool result;
  if (CheckForBadPosition(old_position, new_position, &result)) {
    return result;
  }
  // Correctly calculating the distance between two positions isn't necessary
  // given the small distances we're interested in.
  double delta = fabs(new_position.latitude - old_position.latitude) +
                 fabs(new_position.longitude - old_position.longitude);
  // Convert to metres. 1 second of arc of latitude (or longitude at the
  // equator) is 1 nautical mile or 1852m.
  delta *= 60 * 1852;
  // The threshold is when the distance between the two positions exceeds the
  // worse (larger value) of the two accuracies.
  double max_accuracy = std::max(old_position.accuracy, new_position.accuracy);
  return delta > max_accuracy;
}

static bool IsNewPositionMoreAccurate(const Position &old_position,
                                      const Position &new_position) {
  bool result;
  if (CheckForBadPosition(old_position, new_position, &result)) {
    return result;
  }
  return new_position.accuracy < old_position.accuracy;
}

static bool IsNewPositionMoreTimely(const Position &old_position,
                                    const Position &new_position) {
  bool result;
  if (CheckForBadPosition(old_position, new_position, &result)) {
    return result;
  }
  return GetCurrentTimeMillis() - old_position.timestamp >
      kMaximumPositionFixAge;
}

static bool AcquirePermissionForLocationData(ModuleImplBaseClass *geo_module,
                                             JsCallContext *context) {
  if (!geo_module->GetPermissionsManager()->AcquirePermission(
      PermissionsDB::PERMISSION_LOCATION_DATA,
      geo_module->EnvPageBrowsingContext())) {
    std::string16 error = STRING16(L"Page does not have permission to access "
                                   L"location information using "
                                   PRODUCT_FRIENDLY_NAME);
    context->SetException(error);
    return false;
  }
  return true;
}

bool CreateJavaScriptAddressObject(const Address &address,
                                   JsObject *address_object) {
  assert(address_object);

  bool result = true;
  result &= SetObjectPropertyIfValidString(STRING16(L"streetNumber"),
                                           address.street_number,
                                           address_object);
  result &= SetObjectPropertyIfValidString(STRING16(L"street"),
                                           address.street,
                                           address_object);
  result &= SetObjectPropertyIfValidString(STRING16(L"premises"),
                                           address.premises,
                                           address_object);
  result &= SetObjectPropertyIfValidString(STRING16(L"city"),
                                           address.city,
                                           address_object);
  result &= SetObjectPropertyIfValidString(STRING16(L"county"),
                                           address.county,
                                           address_object);
  result &= SetObjectPropertyIfValidString(STRING16(L"region"),
                                           address.region,
                                           address_object);
  result &= SetObjectPropertyIfValidString(STRING16(L"country"),
                                           address.country,
                                           address_object);
  result &= SetObjectPropertyIfValidString(STRING16(L"countryCode"),
                                           address.country_code,
                                           address_object);
  result &= SetObjectPropertyIfValidString(STRING16(L"postalCode"),
                                           address.postal_code,
                                           address_object);
  return result;
}
