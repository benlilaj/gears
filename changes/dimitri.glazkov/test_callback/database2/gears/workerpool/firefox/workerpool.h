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

#ifndef GEARS_WORKERPOOL_FIREFOX_WORKERPOOL_H__
#define GEARS_WORKERPOOL_FIREFOX_WORKERPOOL_H__

#include <vector>
#include <gecko_sdk/include/nsComponentManagerUtils.h>
#include <gecko_internal/nsIVariant.h>

#include "genfiles/workerpool.h"
#include "gears/base/common/base_class.h"
#include "gears/base/common/common.h"
#include "gears/base/common/js_marshal.h"
#include "gears/base/common/js_runner.h"
#include "gears/base/common/mutex.h"
#include "gears/base/common/security_model.h"
#include "gears/base/common/string16.h"

// Object identifiers
extern const char *kGearsWorkerPoolClassName;
extern const nsCID kGearsWorkerPoolClassId;

class PoolThreadsManager;
struct WorkerPoolMessage;
struct JavaScriptWorkerInfo;
struct ThreadsEvent;


class GearsWorkerPool
    : public ModuleImplBaseClass,
      public GearsWorkerPoolInterface,
      public JsEventHandlerInterface {
 public:
  NS_DECL_ISUPPORTS
  GEARS_IMPL_BASECLASS
  // End boilerplate code. Begin interface.

  // Need a default constructor to instance objects from the Factory.
  GearsWorkerPool();
  ~GearsWorkerPool();

  NS_IMETHOD CreateWorker(//const nsAString &full_script
                          PRInt32 *retval);
  NS_IMETHOD CreateWorkerFromUrl(//const nsAString &url
                                 PRInt32 *retval);
  NS_IMETHOD AllowCrossOrigin();
  NS_IMETHOD SendMessage(//const variant &message_body
                         //PRInt32 dest_worker_id
                        );
  NS_IMETHOD SetOnmessage(nsIVariant *in_handler);
  NS_IMETHOD GetOnmessage(nsIVariant **out_handler);
  NS_IMETHOD SetOnerror(nsIVariant *in_handler);
  NS_IMETHOD GetOnerror(nsIVariant **out_handler);
#ifdef DEBUG
  NS_IMETHOD ForceGC();
#endif

  friend class PoolThreadsManager; // for SetThreadsManager()

 private:
  void Initialize(); // okay to call this multiple times
  void SetThreadsManager(PoolThreadsManager *manager);

  void HandleEvent(JsEventType event_type);

  PoolThreadsManager *threads_manager_;
  bool owns_threads_manager_;
  scoped_ptr<JsEventMonitor> unload_monitor_;

  DISALLOW_EVIL_CONSTRUCTORS(GearsWorkerPool);
};


class PoolThreadsManager
    : JsErrorHandlerInterface {
  friend struct ThreadsEvent; // for OnReceiveThreadsEvent
 public:
  PoolThreadsManager(const SecurityOrigin &page_security_origin,
                     JsRunnerInterface *root_js_runner);

  // We handle the lifetime of the PoolThreadsMananger using ref-counting. 
  // When all references go away, the PoolThreadsManager deletes itself.
  // NOTE: each worker will add (and release) multiple references.
  void AddWorkerRef();
  void ReleaseWorkerRef();

  bool SetCurrentThreadMessageHandler(JsRootedCallback *handler);
  bool SetCurrentThreadErrorHandler(JsRootedCallback *handler);
  bool CreateThread(const char16 *url_or_full_script, bool is_param_script,
                    int *worker_id);
  void AllowCrossOrigin();
  void HandleError(const JsErrorInfo &message);
  bool PutPoolMessage(MarshaledJsToken *mjt, const char16 *text,
                      int dest_worker_id, const SecurityOrigin &src_origin);

  // Worker initialization that must be done from the worker's thread.
  bool InitWorkerThread(JavaScriptWorkerInfo *wi);
  void UninitWorkerThread();

  void ShutDown();
#ifdef DEBUG
  void ForceGCCurrentThread();
#endif

  const SecurityOrigin& page_security_origin() { return page_security_origin_; }

 private:
  ~PoolThreadsManager();

  // Gets the id of the worker associated with the current thread. Caller must
  // acquire the mutex.
  int GetCurrentPoolWorkerId();
  WorkerPoolMessage *GetPoolMessage();
  bool InvokeOnErrorHandler(JavaScriptWorkerInfo *wi,
                            const JsErrorInfo &error_info);

  static void JavaScriptThreadEntry(void *args);
  static bool SetupJsRunner(JsRunnerInterface *js_runner,
                            JavaScriptWorkerInfo *wi);
  static void *OnReceiveThreadsEvent(ThreadsEvent *event);

  // Helpers for processing events received from other workers.
  void ProcessMessage(JavaScriptWorkerInfo *wi,
                      const WorkerPoolMessage &msg);
  void ProcessError(JavaScriptWorkerInfo *wi,
                    const WorkerPoolMessage &msg);

  int num_workers_; // used by Add/ReleaseWorkerRef()
  bool is_shutting_down_;

  std::vector<PRThread*> worker_id_to_os_thread_id_;
  // this _must_ be a vector of pointers, since each worker references its
  // JavaScriptWorkerInfo, but STL vector realloc can move its elements.
  std::vector<JavaScriptWorkerInfo*> worker_info_;

  Mutex mutex_;  // for exclusive access to all class methods and data

  SecurityOrigin page_security_origin_;

  DISALLOW_EVIL_CONSTRUCTORS(PoolThreadsManager);
};


#endif // GEARS_WORKERPOOL_FIREFOX_WORKERPOOL_H__