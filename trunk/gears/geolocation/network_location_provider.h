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

#ifndef GEARS_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H__
#define GEARS_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H__

#include "gears/base/common/common.h"
#include "gears/base/common/mutex.h"
#include "gears/base/common/string16.h"
#include "gears/geolocation/device_data_provider.h"
#include "gears/geolocation/location_provider.h"
#include "gears/geolocation/network_location_request.h"
#include "gears/geolocation/thread.h"

class NetworkLocationProvider
    : public LocationProviderBase,
      public DeviceDataProviderBase<RadioData>::ListenerInterface,
      public DeviceDataProviderBase<WifiData>::ListenerInterface,
      public NetworkLocationRequest::ListenerInterface,
      public Thread {
 public:
  NetworkLocationProvider(const std::string16 &url,
                          const std::string16 &host_name);
  virtual ~NetworkLocationProvider();

  // Override LocationProviderBase implementation.
  virtual void AddListener(LocationProviderBase::ListenerInterface *listener);

  // LocationProviderBase implementation
  virtual void GetPosition(Position *position);

 private:
  // DeviceDataProviderBase::ListenerInterface implementation.
  virtual void DeviceDataUpdateAvailable(
      DeviceDataProviderBase<RadioData> *provider);
  virtual void DeviceDataUpdateAvailable(
      DeviceDataProviderBase<WifiData> *provider);

  // NetworkLocationRequest::ListenerInterface implementation.
  virtual void LocationResponseAvailable(const Position &position);

  // Thread implementation
  virtual void Run();

  // Internal helper used by worker thread to make a network request
  bool MakeRequest();

  // Internal helper used by DeviceDataProviderBase
  void DeviceDataUpdateAvailableImpl();

  // The network location request object and the url and host name it will use.
  NetworkLocationRequest *request_;
  std::string16 url_;
  std::string16 host_name_;

  // The device data providers
  DeviceDataProviderBase<RadioData> *radio_data_provider_;
  DeviceDataProviderBase<WifiData> *wifi_data_provider_;

  // The radio and wifi data, flags to indicate if each data set is complete,
  // and their guarding mutex.
  RadioData radio_data_;
  WifiData wifi_data_;
  bool is_radio_data_complete_;
  bool is_wifi_data_complete_;
  Mutex data_mutex_;

  // The timestamp for the latest device data update.
  int64 timestamp_;

  // Parameters for the network request
  bool request_address_;
  std::string16 address_language_;

  // The current best position estimate and its guarding mutex
  Position position_;
  Mutex position_mutex_;

  // The event used to notify this object's (ie the network location provider)
  // worker thread about changes in available data, the network request status
  // and whether we're shutting down. The booleans are used to indicate what the
  // event signals.
  Event thread_notification_event_;
  bool is_shutting_down_;
  bool is_new_data_available_;
  bool is_last_request_complete_;
  bool is_new_listener_waiting_;

  DISALLOW_EVIL_CONSTRUCTORS(NetworkLocationProvider);
};

#endif  // GEARS_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H__
