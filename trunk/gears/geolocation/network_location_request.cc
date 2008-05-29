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

#ifdef OFFICIAL_BUILD
// The Geolocation API has not been finalized for official builds.
#else

#include "gears/geolocation/network_location_request.h"

#include "gears/blob/buffer_blob.h"
#include "gears/localserver/common/http_constants.h"
#include "third_party/jsoncpp/reader.h"
#include "third_party/jsoncpp/value.h"
#include "third_party/jsoncpp/writer.h"

static const char *kGearsNetworkLocationProtocolVersion = "1.0";

// Local functions.

static bool FormRequestBody(const std::string16 &host_name,
                            const RadioData &radio_data,
                            const WifiData &wifi_data,
                            bool request_address,
                            std::string16 address_language,
                            double latitude,
                            double longitude,
                            scoped_refptr<BlobInterface> *blob);
static bool GetLocationFromResponse(const std::vector<uint8> &response,
                                    Position *position);

// NetworkLocationRequest

// static
NetworkLocationRequest* NetworkLocationRequest::Create(
    const std::string16 &url,
    const std::string16 &host_name,
    ListenerInterface *listener) {
  return new NetworkLocationRequest(url, host_name, listener);
}

NetworkLocationRequest::NetworkLocationRequest(const std::string16 &url,
                                               const std::string16 &host_name,
                                               ListenerInterface *listener)
    : listener_(listener), url_(url), host_name_(host_name) {
  if (!Init()) {
    assert(false);
  }
}

bool NetworkLocationRequest::MakeRequest(const RadioData &radio_data,
                                         const WifiData &wifi_data,
                                         bool request_address,
                                         std::string16 address_language,
                                         double latitude,
                                         double longitude,
                                         int64 timestamp) {
  if (!FormRequestBody(host_name_, radio_data, wifi_data, request_address,
                       address_language, latitude, longitude, &post_body_)) {
    return false;
  }
  timestamp_ = timestamp;
  // This will fail if there's a request currently in progress.
  return Start();
}

// AsyncTask implementation.

void NetworkLocationRequest::Run() {
  WebCacheDB::PayloadInfo payload;
  if (!HttpPost(url_.c_str(),
                false,             // Not capturing, so follow redirects
                NULL,              // reason_header_value
                NULL,              // mod_since_date
                NULL,              // required_cookie
                post_body_.get(),
                &payload,
                NULL,              // was_redirected
                NULL,              // full_redirect_url
                NULL)) {           // error_message
    LOG(("NetworkLocationRequest::Run() : Failed to make HttpPost request.\n"));
    return;
  }
  if (listener_) {
    Position position;
    if (payload.status_code == HttpConstants::HTTP_OK) {
      // If HttpPost succeeded, payload.data is guaranteed to be non-NULL,
      // even if the vector it points to is empty.
      assert(payload.data.get());
      if (GetLocationFromResponse(*payload.data.get(), &position)) {
        // We use the timestamp from the device data that was used to generate
        // this position fix.
        position.timestamp = timestamp_;
      } else {
        // If we failed to parse the repsonse, we call back with an invalid
        // position.
        LOG(("NetworkLocationRequest::Run() : Failed to get location from "
             "response.\n"));
      }
    } else {
      // If the response was bad, we call back with an invalid position.
      LOG(("NetworkLocationRequest::Run() : HttpPost response was bad.\n"));
    }
    // The listener callback may trigger the cleaning up of the request that
    // contains the network location provider, which may cause
    // StopThreadAndDelete to be called on this object. So we must signal
    // completion before we make the call.
    run_complete_event_.Signal();
    LOG(("NetworkLocationRequest::Run() : Calling listener with position.\n"));
    listener_->LocationResponseAvailable(position);
  } else {
    run_complete_event_.Signal();
  }
}

void NetworkLocationRequest::StopThreadAndDelete() {
  Abort();
  run_complete_event_.Wait();
  DeleteWhenDone();
}

// Local functions.

static const char16* RadioTypeToString(RadioType type) {
  switch (type) {
    case RADIO_TYPE_UNKNOWN:
      return STRING16(L"unknown");
    case RADIO_TYPE_GSM:
      return STRING16(L"gsm");
    case RADIO_TYPE_CDMA:
      return STRING16(L"cdma");
    case RADIO_TYPE_WCDMA:
      return STRING16(L"wcdma");
    default:
      assert(false);
  }
  return NULL;
}

// Adds a string if it's valid.
static void AddString(const std::string &property_name,
                      const std::string16 &value,
                      Json::Value *object) {
  assert(object);
  assert(object->isObject());
  if (!value.empty()) {
    std::string value_utf8;
    if (String16ToUTF8(value.c_str(), value.size(), &value_utf8)) {
      (*object)[property_name] = Json::Value(value_utf8);
    }
  }
}

// Adds an integer if it's valid.
static void AddInteger(const std::string &property_name,
                       const int &value,
                       Json::Value *object) {
  assert(object);
  assert(object->isObject());
  if (kint32min != value) {
    (*object)[property_name] = Json::Value(value);
  }
}

// Adds an angle as a double if it's valid.
static void AddAngle(const std::string &property_name,
                     const double &value,
                     Json::Value *object) {
  assert(object);
  assert(object->isObject());
  if (value >= -180.0 && value <= 180.0) {
    (*object)[property_name] = Json::Value(value);
  }
}

static bool FormRequestBody(const std::string16 &host_name,
                            const RadioData &radio_data,
                            const WifiData &wifi_data,
                            bool request_address,
                            std::string16 address_language,
                            double latitude,
                            double longitude,
                            scoped_refptr<BlobInterface> *blob) {
  assert(blob);
  Json::Value body_object;
  assert(body_object.isObject());
  // Version and host are required.
  if (host_name.empty()) {
    return false;
  }
  body_object["version"] = Json::Value(kGearsNetworkLocationProtocolVersion);
  AddString("host", host_name, &body_object);
  AddInteger("home_mobile_country_code", radio_data.home_mcc, &body_object);
  AddInteger("home_mobile_network_code", radio_data.home_mnc, &body_object);
  AddString("radio_type", RadioTypeToString(radio_data.radio_type),
            &body_object);
  AddString("carrier", radio_data.carrier, &body_object);
  body_object["request_address"] = request_address;
  AddString("address_language", address_language, &body_object);

  Json::Value location;
  AddAngle("latitude", latitude, &location);
  AddAngle("longitude", longitude, &location);
  body_object["location"] = location;

  Json::Value cell_towers;
  assert(cell_towers.isArray());
  for (int i = 0; i < static_cast<int>(radio_data.cell_data.size()); ++i) {
    Json::Value cell_tower;
    assert(cell_tower.isObject());
    AddInteger("cell_id", radio_data.cell_data[i].cid, &cell_tower);
    AddInteger("location_area_code", radio_data.cell_data[i].lac, &cell_tower);
    AddInteger("mobile_country_code", radio_data.cell_data[i].mcc, &cell_tower);
    AddInteger("mobile_network_code", radio_data.cell_data[i].mnc, &cell_tower);
    AddInteger("age", radio_data.cell_data[i].age, &cell_tower);
    AddInteger("signal_strength", radio_data.cell_data[i].rss, &cell_tower);
    AddInteger("timing_advance", radio_data.cell_data[i].adv, &cell_tower);
    cell_towers[i] = cell_tower;
  }
  body_object["cell_towers"] = cell_towers;

  Json::Value wifi_towers;
  assert(wifi_towers.isArray());
  for (int i = 0;
       i < static_cast<int>(wifi_data.access_point_data.size());
       ++i) {
    Json::Value wifi_tower;
    assert(wifi_tower.isObject());
    AddString("mac_address", wifi_data.access_point_data[i].mac, &wifi_tower);
    AddInteger("signal_strength", wifi_data.access_point_data[i].rss,
               &wifi_tower);
    AddInteger("age", wifi_data.access_point_data[i].age, &wifi_tower);
    AddInteger("channel", wifi_data.access_point_data[i].cha, &wifi_tower);
    AddInteger("signal_to_noise", wifi_data.access_point_data[i].snr, &wifi_tower);
    AddString("ssid", wifi_data.access_point_data[i].ssi, &wifi_tower);
    wifi_towers[i] = wifi_tower;
  }
  body_object["wifi_towers"] = wifi_towers;

  Json::FastWriter writer;
  std::string body_string = writer.write(body_object);
  blob->reset(new BufferBlob(body_string.c_str(), body_string.size()));
  return true;
}

// The JsValue::asXXX() methods return zero if a property isn't specified. For
// our purposes, zero is a valid value, so we have to test for existence.

// Gets an integer if it's present.
static bool GetAsInt(const Json::Value &object,
                     const std::string &property_name,
                     int *out) {
  assert(out);
  if (!object[property_name].isInt()) {
    return false;
  }
  *out = object[property_name].asInt();
  return true;
}

// Gets a string if it's present.
static bool GetAsString(const Json::Value &object,
                        const std::string &property_name,
                        std::string16 *out) {
  assert(out);
  if (!object[property_name].isString()) {
    return false;
  }
  std::string out_utf8 = object[property_name].asString();
  return UTF8ToString16(out_utf8.c_str(), out_utf8.size(), out);
}

static bool GetLocationFromResponse(const std::vector<uint8> &response,
                                    Position *position) {
  assert(position);
  if (response.empty()) {
    LOG(("GetLocationFromResponse() : Response was empty.\n"));
    return false;
  }
  std::string response_string;
  response_string.assign(reinterpret_cast<const char*>(&response[0]),
                         response.size());
  // Parse the response, ignoring comments.
  Json::Reader reader;
  Json::Value response_object;
  if (!reader.parse(response_string, response_object, false)) {
    LOG(("GetLocationFromResponse() : Failed to parse response : %s.\n",
         reader.getFormatedErrorMessages().c_str()));
    return false;
  }
  assert(response_object.isObject());
  Json::Value location = response_object["location"];
  if (!location.isObject()) {
    return false;
  }
  // latitude and longitude fields are required.
  if (!location["latitude"].isDouble() || !location["longitude"].isDouble()) {
    return false;
  }
  position->latitude = location["latitude"].asDouble();
  position->longitude = location["longitude"].asDouble();
  // Other fields are optional.
  GetAsInt(location, "altitude", &position->altitude);
  GetAsInt(location, "horizontal_accuracy", &position->horizontal_accuracy);
  GetAsInt(location, "vertical_accuracy", &position->vertical_accuracy);
  Json::Value address = location["address"];
  if (address.isObject()) {
    GetAsString(address, "street_number", &position->address.street_number);
    GetAsString(address, "street", &position->address.street);
    GetAsString(address, "premises", &position->address.premises);
    GetAsString(address, "city", &position->address.city);
    GetAsString(address, "county", &position->address.county);
    GetAsString(address, "region", &position->address.region);
    GetAsString(address, "country", &position->address.country);
    GetAsString(address, "country_code", &position->address.country_code);
    GetAsString(address, "postal_code", &position->address.postal_code);
  }
  return true;
}

#ifdef USING_CCTESTS
// These methods are used only for testing as a means to access the static
// functions defined here.
bool FormRequestBodyTest(const std::string16 &host_name,
                         const RadioData &radio_data,
                         const WifiData &wifi_data,
                         bool request_address,
                         std::string16 address_language,
                         double latitude,
                         double longitude,
                         scoped_refptr<BlobInterface> *blob) {
  return FormRequestBody(host_name, radio_data, wifi_data, request_address,
                         address_language, latitude, longitude, blob);
}
bool GetLocationFromResponseTest(const std::vector<uint8> &response,
                                 Position *position) {
  return GetLocationFromResponse(response, position);
}
#endif

#endif  // OFFICIAL_BUILD