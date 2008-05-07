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

#include "gears/geolocation/device_data_provider.h"

// static
template <>
DeviceDataProviderBase<RadioData>* DeviceDataProviderBase<RadioData>::instance =
    NULL;

// static
template <>
Mutex DeviceDataProviderBase<RadioData>::instance_mutex;

// static
template <>
DeviceDataProviderBase<WifiData>* DeviceDataProviderBase<WifiData>::instance =
    NULL;

// static
template <>
Mutex DeviceDataProviderBase<WifiData>::instance_mutex;

// MockRadioDataProvider and MockWifiDataProvider are currently only provided
// for Win32.
#if USING_CCTESTS && defined(WIN32)

#include <assert.h>
#include <atlsync.h>  // For CEvent.
#include "gears/localserver/common/async_task.h"

// A mock implementation of DeviceDataProviderBase for testing. It simply calls
// back once every second with constant data.

template<typename DataType>
class MockDeviceDataProvider
    : public DeviceDataProviderBase<DataType>,
      public AsyncTask {
 public:
  // Allow DeviceDataProviderBase<DataType>::Create() to access our private
  // constructor.
  friend DeviceDataProviderBase<DataType>;
 protected:
  // Protected constructor and destructor, callers access singleton through
  // Register and Unregister.
  MockDeviceDataProvider() {
    if (!Init()) {
      assert(false);
    }
    if (!Start()) {
      assert(false);
    }
  }
  virtual ~MockDeviceDataProvider() {}
 private:
  // DeviceDataProviderBase<DataType> implementation.
  void StopAndDelete() {
    DeleteWhenDone();
    stop_event_.Set();
    WaitForSingleObject(run_complete_event_, INFINITE);
  }
  // AsyncTask implementation.
  virtual void Run() {
    while (true) {
      if (WaitForSingleObject(stop_event_, 1000) == WAIT_OBJECT_0) {
        break;
      }
      listeners_mutex_.Lock();
      for (typename ListenersSet::const_iterator iter = listeners_.begin();
           iter != listeners_.end();
           ++iter) {
        (*iter)->DeviceDataUpdateAvailable(this);
      }
      listeners_mutex_.Unlock();
    }
    run_complete_event_.Set();
  }
  bool Init() {
    if (!stop_event_.Create(NULL, FALSE, FALSE, NULL)) {
      return false;
    }
    if (!run_complete_event_.Create(NULL, FALSE, FALSE, NULL)) {
      return false;
    }
    return AsyncTask::Init();
  }

  CEvent stop_event_;
  CEvent run_complete_event_;
  DISALLOW_EVIL_CONSTRUCTORS(MockDeviceDataProvider);
};

class MockRadioDataProvider : public MockDeviceDataProvider<RadioData> {
 private:
  // DeviceDataProviderBase<RadioData> implementation.
  virtual bool GetData(RadioData *data) {
    assert(data);
    // The following are real-world values, captured in May 2008.
    CellData cell_data;
    cell_data.cid = 23874;
    cell_data.lac = 98;
    cell_data.mnc = 15;
    cell_data.mcc = 234;
    cell_data.rss = -65;
    data->cell_data.clear();
    data->cell_data.push_back(cell_data);
    data->radio_type = RADIO_TYPE_GSM;
    // We always have all the data we can get, so return true.
    return true;
  }
};

class MockWifiDataProvider : public MockDeviceDataProvider<WifiData> {
 private:
  // DeviceDataProviderBase<WifiData> implementation.
  virtual bool GetData(WifiData *data) {
    assert(data);
    AccessPointData access_point_data;
    access_point_data.mac = STRING16(L"test mac");
    data->access_point_data.clear();
    data->access_point_data.push_back(access_point_data);
    // We always have all the data we can get, so return true.
    return true;
  }
};

// static
template <>
DeviceDataProviderBase<RadioData>* DeviceDataProviderBase<RadioData>::Create() {
  return new MockRadioDataProvider();
}

// static
template <>
DeviceDataProviderBase<WifiData>* DeviceDataProviderBase<WifiData>::Create() {
  return new MockWifiDataProvider();
}

#else

// Temporarily implement these methods to avoid link errors.
// TODO(steveblock): Implement DeviceDataProviderBase for other platforms.

// static
template <>
DeviceDataProviderBase<RadioData>* DeviceDataProviderBase<RadioData>::Create() {
  assert(false);
  return NULL;
}

// static
template <>
DeviceDataProviderBase<WifiData>* DeviceDataProviderBase<WifiData>::Create() {
  assert(false);
  return NULL;
}

#endif  // USING_CCTESTS && WIN32