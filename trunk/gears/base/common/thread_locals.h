// Copyright 2006, Google Inc.
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

#ifndef GEARS_BASE_COMMON_THREAD_LOCALS_H__
#define GEARS_BASE_COMMON_THREAD_LOCALS_H__

#include <map>
#include "gears/base/common/common.h"

#if BROWSER_SAFARI
#include <pthread.h>
#endif

// TODO(mpcomplete): implement these.
#if BROWSER_NPAPI && defined(WIN32)
#define BROWSER_IE 1
#endif

//------------------------------------------------------------------------------
// Provides for per thread-local storage in a win32 DLL. This classes allocates
// a single TLS index from the OS, and keeps multiple entries in a map
// stored in that per-thread slot.
//
// Users of this class should follow the convention module:name to avoid 
// conflicts in keys. For example, "base:permissions".
//------------------------------------------------------------------------------
class ThreadLocals {
 public:

  // Destructor callback used to destroy a value when a thread terminates.
  typedef void (*DestructorCallback)(void*);

  // Returns the thread-local value for the given key. If no value
  // has been stored with this key, returns NULL.
  // @key, the key of the value to retrieve
  static void *GetValue(const std::string &key);

  // Returns whether or not a thread-local value exists for the given key.
  // @key, the key of the value to test
  static bool HasValue(const std::string &key);

  // Sets the thead local value and its destructor for the given key. When the
  // thread terminates, the destructor (if non-null) will be called.  If a value
  // for the key already exists, the old value is destroyed and its entry is
  // replaced with the new value.
  // @key, the key of the value to set
  // @value, the value to store for the current thread
  // @destructor, optional destructor callback that will be invoked
  //              when the value is cleared (by DestroyValue) or when
  //              the thread dies, can be null.  Note that the thread
  //              the destructor is called in may differ across
  //              platforms.  For DestroyValue, it should always be
  //              the same thread SetValue was called from.  On thread
  //              death under Windows, the destructor is called in the
  //              same thread SetValue() was called from.  On thread
  //              death under Firefox on Linux and MacOS X, the
  //              destructor is called in the joining thread.
  static void SetValue(const std::string &key,
                       void *value,
                       DestructorCallback destructor);

  // Destroys the thread-local value for the given key and clears its entry.
  // @key, the key of value to destroy
  static void DestroyValue(const std::string &key);

 private:
  // We keep an entry of this type for each value in the per thread map.
  struct Entry {
    Entry() : value_(NULL), destructor_(NULL) {}
    Entry(void *value, DestructorCallback destructor) :
      value_(value), destructor_(destructor) {}
    void *value_;
    DestructorCallback destructor_;
  };
  typedef std::map< std::string, Entry > Map;

  // Returns the map associated with the currently executing thread, optionally
  // creating the map if one does not already exist.
  static Map *GetMap(bool createIfNeeded);

  static void ClearMap();
  static void DestroyMap(Map* map);
  static void SetTlsMap(Map* map);
  static Map* GetTlsMap();

#if BROWSER_IE
  // We use one thread-local storage slot from the OS and keeps a map
  // in that slot. This is the index of that slot as returned by TlsAlloc.
  static DWORD tls_index_;

  // Some private methods of this class need to be called by the containing DLL.
  // We declare the function that calls these methods a friend.
  // @see base/ie/module.cc
  friend inline BOOL MyDllMain(HANDLE instance,
                               DWORD reason,
                               LPVOID reserved);

  // Should be called when the DLL is loading. If FALSE is returned, the DLL
  // should fail to load.
  static BOOL HandleProcessAttached();

  // Should be called when the DLL is unloading or the process is existing.
  static void HandleProcessDetached();

  // Should be called when a thread is terminating.
  static void HandleThreadDetached();
#elif BROWSER_FF
  // We use one thread-local storage slot from the OS and keeps a map
  // in that slot. This is the index of that slot as returned by TlsAlloc.
  static PRUintn tls_index_;

  // Some private methods of this class need to be called by the containing DLL.
  // We declare the function that calls these methods a friend.
  // @see module.cc
  friend nsresult PR_CALLBACK ScourModuleConstructor(class nsIModule* self);

  // We need the ThreadRecycler to have access to clean up thread local
  // storage.
  friend class JsThreadRecycler;

  // Should be called when the DLL is loading. If an error is returned the DLL
  // should fail to load.
  static nsresult HandleModuleConstructed();

  // Called by NSPR when the value in our TLS slot is destructed.
  static void PR_CALLBACK TlsDestructor(void *priv);
#elif BROWSER_SAFARI
  // The pthread key to use to save the map
  static pthread_key_t tls_index_;

  // Ensure that the map key is initialized only once
  static pthread_once_t tls_index_init_;

  // Initialize the key
  static void InitializeKey();
  
  // Finalize the key
  static void FinalizeKey(void *context);
#endif
};

// TODO(mpcomplete): remove.
#if BROWSER_NPAPI
#undef BROWSER_IE
#endif

#endif  // GEARS_BASE_COMMON_THREAD_LOCALS_H__