// Copyright 2007, Google Inc.
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

#include "gears/localserver/npapi/async_task_np.h"

#include "gears/base/common/async_router.h"
#include "gears/base/common/atomic_ops.h"
#include "gears/base/npapi/browser_utils.h"
#include "gears/localserver/common/critical_section.h"
#include "gears/localserver/common/http_constants.h"
#include "gears/localserver/common/http_cookies.h"

const char16 *AsyncTask::kCookieRequiredErrorMessage =
                  STRING16(L"Required cookie is not present");

const char16 *kIsOfflineErrorMessage =
                  STRING16(L"The browser is offline");

//------------------------------------------------------------------------------
// AsyncTask
//------------------------------------------------------------------------------
AsyncTask::AsyncTask()
    : is_initialized_(false),
      is_aborted_(false),
      delete_when_done_(false),
      ready_state_changed_signalled_(false),
      abort_signalled_(false),
      listener_thread_id_(0),
      task_thread_id_(0),
      listener_(NULL),
      thread_(NULL) {
  Ref();
}

//------------------------------------------------------------------------------
// ~AsyncTask
//------------------------------------------------------------------------------
AsyncTask::~AsyncTask() {
  assert(!thread_);
  assert(GetRef() == 0 || 
         (GetRef() == 1 && !delete_when_done_));  
}

//------------------------------------------------------------------------------
// Init
//------------------------------------------------------------------------------
bool AsyncTask::Init() {
  if (is_initialized_) {
    assert(!is_initialized_);
    return false;
  }

  ThreadMessageQueue::GetInstance()->InitThreadMessageQueue();

  is_aborted_ = false;
  is_initialized_ = true;
  listener_thread_id_ = ThreadMessageQueue::GetInstance()->GetCurrentThreadId();
  return true;
}

//------------------------------------------------------------------------------
// SetListener
//------------------------------------------------------------------------------
void AsyncTask::SetListener(Listener *listener) {
  CritSecLock locker(lock_);
  assert(!delete_when_done_);
  assert(IsListenerThread());
  listener_ = listener;
}

//------------------------------------------------------------------------------
// Start
//------------------------------------------------------------------------------
bool AsyncTask::Start() {
  if (!is_initialized_ || thread_) {
    return false;
  }

  assert(IsListenerThread());

  CritSecLock locker(lock_);
  is_aborted_ = false;
  ready_state_changed_signalled_ = false;
  abort_signalled_ = false;
  unsigned int thread_id;
  thread_ = reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, &ThreadMain,
                                                    this, 0, &thread_id));
  task_thread_id_ = thread_id;

  if (thread_ == NULL) return false;

  Ref();  // reference is removed upon worker thread exit
  return true;
}

//------------------------------------------------------------------------------
// Abort
//------------------------------------------------------------------------------
void AsyncTask::Abort() {
  CritSecLock locker(lock_);
  if (thread_ && !is_aborted_) {
    LOG16((L"AsyncTask::Abort\n"));
    is_aborted_ = true;
    CallAsync(task_thread_id_, kAbortMessageCode, 0);
  }
}

//------------------------------------------------------------------------------
// DeleteWhenDone
//------------------------------------------------------------------------------
void AsyncTask::DeleteWhenDone() {
  assert(!delete_when_done_);
  assert(IsListenerThread());

  LOG(("AsyncTask::DeleteWhenDone\n"));
  CritSecLock locker(lock_);
  SetListener(NULL);
  delete_when_done_ = true;

  // We have to call unlock prior to calling Unref 
  // otherwise the locker would try to access deleted memory, &lock_,
  // after it's been freed.
  locker.Unlock();
  Unref();  // remove the reference added by the constructor
}


//------------------------------------------------------------------------------
// ThreadMain
//------------------------------------------------------------------------------
unsigned int __stdcall AsyncTask::ThreadMain(void *task) {
  // If initialization fails, don't return immediately. Let the thread continue
  // to run to let signaling around startup/shutdown performed by clients work
  // properly. Down the road, COM objects will fail to be created or function
  // and the thread will unwind normally.
  if (FAILED(CoInitializeEx(NULL, GEARS_COINIT_THREAD_MODEL))) {
    LOG(("AsyncTask::ThreadMain - failed to initialize new thread.\n"));
  }

  AsyncTask *self = reinterpret_cast<AsyncTask*>(task);

  // Don't run until we're sure all state setup by the Start method initialized.
  {
    CritSecLock locker(self->lock_);
    assert(self->IsTaskThread());
  }

  ThreadMessageQueue::GetInstance()->InitThreadMessageQueue();

  self->Run();

  CloseHandle(self->thread_);
  self->thread_ = NULL;
  self->Unref();  // remove the reference added by the Start

  CoUninitialize();
  return 0;
}

//------------------------------------------------------------------------------
// AsyncTaskFunctor
//------------------------------------------------------------------------------

struct AsyncTaskFunctor : public AsyncFunctor {
public:
  AsyncTaskFunctor(int code, int param, AsyncTask *target)
      : code(code), param(param), target(target) {
    target->Ref();
  }
  ~AsyncTaskFunctor() {
    target->Unref();
  }

  virtual void Run() {
    target->HandleAsync(code, param);
  }

  int code;
  int param;
  AsyncTask *target;
};

void AsyncTask::CallAsync(ThreadId thread_id, int code, int param) {
  AsyncRouter::GetInstance()->CallAsync(
      thread_id, new AsyncTaskFunctor(code, param, this));
}

void AsyncTask::HandleAsync(int code, int param) {
  switch (code) {
  case kAbortMessageCode:
    assert(IsTaskThread());
    abort_signalled_ = true;
    break;
  default:
    assert(IsListenerThread());
    if (listener_) {
      listener_->HandleEvent(code, param, this);
    }
    break;
  }
}

//------------------------------------------------------------------------------
// NotifyListener
//------------------------------------------------------------------------------
void AsyncTask::NotifyListener(int code, int param) {
  CallAsync(listener_thread_id_, code, param);
}

//------------------------------------------------------------------------------
// HttpGet
//------------------------------------------------------------------------------
bool AsyncTask::HttpGet(const char16 *full_url,
                        bool is_capturing,
                        const char16 *reason_header_value,
                        const char16 *if_mod_since_date,
                        const char16 *required_cookie,
                        WebCacheDB::PayloadInfo *payload,
                        bool *was_redirected,
                        std::string16 *full_redirect_url,
                        std::string16 *error_message) {
  return MakeHttpRequest(HttpConstants::kHttpGET,
                         full_url,
                         is_capturing,
                         reason_header_value,
                         if_mod_since_date,
                         required_cookie,
#ifdef OFFICIAL_BUILD
                         // The Blob API has not yet been finalized for official
                         // builds.
#else
                         NULL,
#endif
                         payload,
                         was_redirected,
                         full_redirect_url,
                         error_message);
}

#ifdef OFFICIAL_BUILD
// The Blob API has not yet been finalized for official builds.
#else
//------------------------------------------------------------------------------
// HttpGet
//------------------------------------------------------------------------------
bool AsyncTask::HttpPost(const char16 *full_url,
                         bool is_capturing,
                         const char16 *reason_header_value,
                         const char16 *if_mod_since_date,
                         const char16 *required_cookie,
                         BlobInterface *post_body,
                         WebCacheDB::PayloadInfo *payload,
                         bool *was_redirected,
                         std::string16 *full_redirect_url,
                         std::string16 *error_message) {
  return MakeHttpRequest(HttpConstants::kHttpPOST,
                         full_url,
                         is_capturing,
                         reason_header_value,
                         if_mod_since_date,
                         required_cookie,
                         post_body,
                         payload,
                         was_redirected,
                         full_redirect_url,
                         error_message);
}
#endif

//------------------------------------------------------------------------------
// MakeHttpRequest
//------------------------------------------------------------------------------
bool AsyncTask::MakeHttpRequest(const char16 *method,
                                const char16 *full_url,
                                bool is_capturing,
                                const char16 *reason_header_value,
                                const char16 *if_mod_since_date,
                                const char16 *required_cookie,
#ifdef OFFICIAL_BUILD
                                // The Blob API has not yet been finalized for
                                // official builds.
#else
                                BlobInterface *post_body,
#endif
                                WebCacheDB::PayloadInfo *payload,
                                bool *was_redirected,
                                std::string16 *full_redirect_url,
                                std::string16 *error_message) {
  if (was_redirected) {
    *was_redirected = false;
  }
  if (full_redirect_url) {
    full_redirect_url->clear();
  }
  if (error_message) {
    error_message->clear();
  }

  if (!BrowserUtils::IsOnline()) {
    if (error_message) {
      *error_message = kIsOfflineErrorMessage;
    }
    return false;
  }

  if (required_cookie && required_cookie[0]) {
    std::string16 required_cookie_str(required_cookie);
    std::string16 name, value;
    ParseCookieNameAndValue(required_cookie_str, &name, &value);
    if (value != kNegatedRequiredCookieValue) {
      CookieMap cookie_map;
      cookie_map.LoadMapForUrl(full_url);
      if (!cookie_map.HasLocalServerRequiredCookie(required_cookie_str)) {
        if (error_message) {
          *(error_message) = kCookieRequiredErrorMessage;
        }
        return false;
      }
    }
  }

  scoped_refptr<HttpRequest> http_request;
  if (!HttpRequest::CreateSafeRequest(&http_request)) {
    return false;
  }

  http_request->SetCachingBehavior(HttpRequest::BYPASS_ALL_CACHES);

  if (!http_request->Open(method, full_url, true)) {
    return false;
  }

  if (is_capturing) {
    http_request->SetRedirectBehavior(HttpRequest::FOLLOW_NONE);
    if (!http_request->SetRequestHeader(HttpConstants::kXGoogleGearsHeader,
                                        STRING16(L"1"))) {
      return false;
    }
  }

  if (reason_header_value && reason_header_value[0]) {
    if (!http_request->SetRequestHeader(HttpConstants::kXGearsReasonHeader,
                                        reason_header_value)) {
      return false;
    }
  }

  if (!http_request->SetRequestHeader(HttpConstants::kCacheControlHeader,
                                      HttpConstants::kNoCache)) {
    return false;
  }

  if (!http_request->SetRequestHeader(HttpConstants::kPragmaHeader,
                                      HttpConstants::kNoCache)) {
    return false;
  }

  if (if_mod_since_date && if_mod_since_date[0]) {
    if (!http_request->SetRequestHeader(HttpConstants::kIfModifiedSinceHeader,
                                        if_mod_since_date)) {
      return false;
    }
  }

  payload->data.reset();
  http_request->SetListener(this, false);
  ready_state_changed_signalled_ = false;

  if (is_aborted_) {
    http_request->SetListener(NULL, false);
    return false;
  }

  // Rely on logic inside HttpRequest to check for valid combinations of
  // method and presence of body.
  bool result = false;
#ifdef OFFICIAL_BUILD
  // The Blob API has not yet been finalized for official builds.
  if (false) {
#else
  if (post_body) {
    result = http_request->SendBlob(post_body);
#endif
  } else {
    result = http_request->Send();
  }
  if (!result) {
    http_request->SetListener(NULL, false);
    return false;
  }

  bool done = false;
  MSG msg;
  while (!done && GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);

    if (ready_state_changed_signalled_) {
      ready_state_changed_signalled_ = false;
      // TODO(michaeln): perhaps simplify the HttpRequest interface to
      // include a getResponse(&payload) method?
      HttpRequest::ReadyState state;
      if (http_request->GetReadyState(&state)) {
        if (state == HttpRequest::COMPLETE) {
          done = true;
          if (!is_aborted_) {
            int status;
            if (http_request->GetStatus(&status)) {
              payload->status_code = status;
              if (http_request->GetStatusLine(&payload->status_line)) {
                if (http_request->GetAllResponseHeaders(&payload->headers)) {
                  payload->data.reset(http_request->GetResponseBody());
                }
              }
            }
          }
        }
      } else {
        LOG16((L"AsyncTask - getReadyState failed, aborting request\n"));
        http_request->Abort();
        done = true;
      }
    } else if (abort_signalled_) {
      LOG16((L"AsyncTask - abort event signalled, aborting request\n"));
      abort_signalled_ = false;
      http_request->Abort();
      done = true;
   }
  }

  http_request->SetListener(NULL, false);

  if (http_request->WasRedirected()) {
    if (was_redirected) {
      *was_redirected = true;
    }
    if (full_redirect_url) {
      http_request->GetFinalUrl(full_redirect_url);
    }
  }

  return !is_aborted_ && payload->data.get();
}

//------------------------------------------------------------------------------
// HttpRequest::HttpListener::ReadyStateChanged
//------------------------------------------------------------------------------
void AsyncTask::ReadyStateChanged(HttpRequest *source) {
  assert(IsTaskThread());
  ready_state_changed_signalled_ = true;
}