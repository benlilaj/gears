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
#ifndef GEARS_NOTIFIER_NOTIFICATION_H__
#define GEARS_NOTIFIER_NOTIFICATION_H__

#ifdef OFFICIAL_BUILD
  // The notification API has not been finalized for official builds.
#else

#include <vector>
#if !BROWSER_NONE
#include "gears/base/common/base_class.h"
#endif
#include "gears/base/common/serialization.h"
#include "gears/base/common/string16.h"

struct NotificationAction {
  std::string16 text;
  std::string16 url;
};

// Describes the information about a notification.
// Note: do not forget to increase kNotificationVersion if you make any change
// to this class.
class GearsNotification :
#if !BROWSER_NONE
    public ModuleImplBaseClassVirtual,
#endif
    public Serializable {
 public:
  static const std::string kModuleName;

  GearsNotification();

  //
  // For JavaScript API
  //

#if !BROWSER_NONE
  // IN: nothing
  // OUT: string
  void GetTitle(JsCallContext *context);

  // IN: string
  // OUT: nothing
  void SetTitle(JsCallContext *context);

  // IN: nothing
  // OUT: string
  void GetIcon(JsCallContext *context);

  // IN: string
  // OUT: nothing
  void SetIcon(JsCallContext *context);

  // IN: nothing
  // OUT: string
  void GetId(JsCallContext *context);

  // IN: string
  // OUT: nothing
  void SetId(JsCallContext *context);

  // IN: nothing
  // OUT: string
  void GetSubtitle(JsCallContext *context);

  // IN: string
  // OUT: nothing
  void SetSubtitle(JsCallContext *context);

  // IN: nothing
  // OUT: string
  void GetDescription(JsCallContext *context);

  // IN: string
  // OUT: nothing
  void SetDescription(JsCallContext *context);

  // IN: nothing
  // OUT: date
  void GetDisplayAtTime(JsCallContext *context);

  // IN: date
  // OUT: nothing
  void SetDisplayAtTime(JsCallContext *context);

  // IN: nothing
  // OUT: date
  void GetDisplayUntilTime(JsCallContext *context);

  // IN: date
  // OUT: nothing
  void SetDisplayUntilTime(JsCallContext *context);

  // IN: string text and string url
  // OUT: nothing
  void AddAction(JsCallContext *context);
#endif  // !BROWSER_NONE

  //
  // Serializable interface
  //

  virtual SerializableClassId GetSerializableClassId() {
    return SERIALIZABLE_DESKTOP_NOTIFICATION;
  }
  virtual bool Serialize(Serializer *out);

  virtual bool Deserialize(Deserializer *in);

  static Serializable *New() {
    return new GearsNotification;
  }

  static void RegisterAsSerializable() {
    Serializable::RegisterClass(SERIALIZABLE_DESKTOP_NOTIFICATION, New);
  }

  //
  // Helpers
  //

  void CopyFrom(const GearsNotification& from);

  bool Matches(const std::string16 &service, const std::string16 &id) const {
    return service_ == service && id_ == id;
  }

  //
  // Accessors
  //

  const std::string16& title() const { return title_; }
  const std::string16& subtitle() const { return subtitle_; }
  const std::string16& icon() const { return icon_; }
  const std::string16& service() const { return service_; }
  const std::string16& id() const { return id_; }
  const std::string16& description() const { return description_; }
  int64 display_at_time_ms() const { return display_at_time_ms_; }
  int64 display_until_time_ms() const { return display_until_time_ms_; }
  const std::vector<NotificationAction> &actions() const { return actions_; }

  void set_title(const std::string16& title) { title_ = title; }
  void set_subtitle(const std::string16& subtitle) { subtitle_ = subtitle; }
  void set_icon(const std::string16& icon) { icon_ = icon; }
  void set_service(const std::string16& service) { service_ = service; }
  void set_id(const std::string16& id) { id_ = id; }
  void set_description(const std::string16& description) {
    description_ = description;
  }
  void set_display_at_time_ms(int64 display_at_time_ms) {
    display_at_time_ms_ = display_at_time_ms;
  }
  void set_display_until_time_ms(int64 display_until_time_ms) {
    display_until_time_ms_ = display_until_time_ms;
  }
  std::vector<NotificationAction> *mutable_actions() { return &actions_; }

 private:
  static const int kNotificationVersion = 4;

  // NOTE: Increase the kNotificationVersion every time the serialization is
  // going to produce different result. This most likely includes any change
  // to data members below.
  int version_;
  std::string16 title_;
  std::string16 subtitle_;
  std::string16 icon_;
  std::string16 id_;
  std::string16 service_;
  std::string16 description_;
  int64 display_at_time_ms_;
  int64 display_until_time_ms_;
  std::vector<NotificationAction> actions_;
  // NOTE: Increase the kNotificationVersion every time the serialization is
  // going to produce different result. This most likely includes any change
  // to data members above.

  DISALLOW_EVIL_CONSTRUCTORS(GearsNotification);
};

#endif  // OFFICIAL_BUILD
#endif  // GEARS_NOTIFIER_NOTIFICATION_H__