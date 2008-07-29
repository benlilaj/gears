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

#include "gears/geolocation/location_provider_pool.h"

#include <assert.h>

static const char16 *kGpsString = STRING16(L"GPS");

// Local functions
static std::string16 MakeKey(const std::string16 &type,
                             const std::string16 &host,
                             const std::string16 &language);
static LocationProviderBase *NewProvider(const std::string16 &type,
                                         const std::string16 &host,
                                         const std::string16 &language);

// static member variables
LocationProviderPool LocationProviderPool::instance_;

LocationProviderPool::~LocationProviderPool() {
  assert(providers_.empty());
}

// static
LocationProviderPool *LocationProviderPool::GetInstance() {
  return &instance_;
}

LocationProviderBase *LocationProviderPool::Register(
      const std::string16 &type,
      const std::string16 &host,
      bool request_address,
      const std::string16 &language,
      LocationProviderBase::ListenerInterface *listener) {
  assert(listener);
  MutexLock lock(&providers_mutex_);
  std::string16 key = MakeKey(type, host, language);
  ProviderMap::iterator iter = providers_.find(key);
  if (iter == providers_.end()) {
    LocationProviderBase *provider = NewProvider(type, host, language);
    if (!provider) {
      return NULL;
    }
    std::pair<ProviderMap::iterator, bool> result =
        providers_.insert(
        std::make_pair(key,
        std::make_pair(provider, new RefCount())));
    assert(result.second);
    iter = result.first;
  }
  LocationProviderBase *provider = iter->second.first;
  assert(provider);
  provider->AddListener(listener, request_address);
  RefCount *count = iter->second.second;
  assert(count);
  count->Ref();
  return provider;
}

bool LocationProviderPool::Unregister(
    LocationProviderBase *provider,
    LocationProviderBase::ListenerInterface *listener) {
  assert(provider);
  assert(listener);
  MutexLock lock(&providers_mutex_);
  for (ProviderMap::iterator iter = providers_.begin();
       iter != providers_.end();
       ++iter) {
    LocationProviderBase *current_provider = iter->second.first;
    if (current_provider == provider) {
      current_provider->RemoveListener(listener);
      RefCount *count = iter->second.second;
      assert(count);
      if (count->Unref()) {
        delete current_provider;
        delete count;
        providers_.erase(iter);
      }
      return true;
    }
  }
  return false;
}

// Local functions

static std::string16 MakeKey(const std::string16 &type,
                             const std::string16 &host,
                             const std::string16 &language) {
  // A network location request is made from a specific host. Also, the request
  // includes the address language. Therefore we must key network providers on
  // server URL, host name and language
  if (type == kGpsString) {
    return kGpsString;
  } else {
    std::string16 key = type + STRING16(L" ") + host;
    if (!language.empty()) {
      key += STRING16(L" ") + language;
    }
    return key;
  }
}

static LocationProviderBase *NewProvider(const std::string16 &type,
                                         const std::string16 &host,
                                         const std::string16 &language) {
  if (type == kGpsString) {
    return NewGpsLocationProvider();
  } else {
    return NewNetworkLocationProvider(type, host, language);
  }
}
