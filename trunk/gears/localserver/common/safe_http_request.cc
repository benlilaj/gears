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

#include "gears/localserver/common/safe_http_request.h"

//------------------------------------------------------------------------------
// Constructor, destructor.
//------------------------------------------------------------------------------

SafeHttpRequest::SafeHttpRequest(ThreadId safe_thread_id)
    : was_aborted_(false), was_sent_(false),
      was_response_text_accessed_(false), was_data_available_called_(false),
      listener_(NULL), listener_data_available_enabled_(false), 
      safe_thread_id_(safe_thread_id), apartment_thread_id_(0) {
  ThreadMessageQueue *msg_q = ThreadMessageQueue::GetInstance();
  msg_q->InitThreadMessageQueue();
  apartment_thread_id_ = msg_q->GetCurrentThreadId();
}

SafeHttpRequest::~SafeHttpRequest() {
  assert(!native_http_request_.get());
}
 
//------------------------------------------------------------------------------
// HttpRequest interface methods.
//------------------------------------------------------------------------------

bool SafeHttpRequest::Open(const char16 *method,
                           const char16 *url,
                           bool async) {
  assert(IsApartmentThread());
  // TODO(michaeln): support sync requests
  if (!async)
    return false;
  if (GetState() != UNINITIALIZED)
    return false;
  request_info_.upcoming_ready_state = OPEN;
  request_info_.method = MakeUpperString(std::string16(method));
  request_info_.full_url = url;
  OnReadyStateChangedCall();
  return true;
}

bool SafeHttpRequest::GetResponseBodyAsText(std::string16 *text) {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (!IsValidResponse()) {
    return false;
  }
  was_response_text_accessed_ = true;
  *text = request_info_.response.text;
  return true;
}

bool SafeHttpRequest::GetResponseBody(std::vector<uint8> *body) {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (!IsValidResponse() || (GetState() != COMPLETE)) {
    return false;
  }
  if (request_info_.response.body.get()) {
    body->swap(*request_info_.response.body.get());
  } else {
    body->clear();
  }
  return true;
}

std::vector<uint8> *SafeHttpRequest::GetResponseBody() {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (!IsValidResponse() || (GetState() != COMPLETE)) {
    return NULL;
  }
  return request_info_.response.body.release();
}

bool SafeHttpRequest::GetAllResponseHeaders(std::string16 *headers) {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (!IsValidResponse()) {
    return false;
  }
  *headers = request_info_.response.headers;
  return true;
}

bool SafeHttpRequest::GetResponseHeader(const char16 *name,
                                        std::string16 *header) {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (!IsValidResponse()) {
    return false;
  }

  if (!request_info_.response.parsed_headers.get()) {
    scoped_ptr<HTTPHeaders> parsed_headers(new HTTPHeaders);
    std::string headers_utf8;
    String16ToUTF8(request_info_.response.headers.c_str(), 
                   request_info_.response.headers.length(),
                   &headers_utf8);
    const char *body = headers_utf8.c_str();
    uint32 body_len = headers_utf8.length();
    if (!HTTPUtils::ParseHTTPHeaders(&body, &body_len, parsed_headers.get(),
                                     true)) {   // true means allow_const_cast
      return false;
    }
    request_info_.response.parsed_headers.swap(parsed_headers);
  }

  std::string name_utf8;
  String16ToUTF8(name, &name_utf8);
  const char *value_utf8 = request_info_.response.parsed_headers->
      GetHeader(name_utf8.c_str());
  std::string16 value;
  UTF8ToString16(value_utf8 ? value_utf8 : "", &value);
  *header = value;
  return true;
}

bool SafeHttpRequest::GetStatus(int *status) {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (!IsValidResponse()) {
    return false;
  }
  *status = request_info_.response.status;
  return true;
}

bool SafeHttpRequest::GetStatusText(std::string16 *status_text) {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (!IsValidResponse())
    return false;
  *status_text = request_info_.response.status_text;
  return true;
}

bool SafeHttpRequest::GetStatusLine(std::string16 *status_line) {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (!IsValidResponse())
    return false;
  *status_line = request_info_.response.status_line;
  return true;
}

bool SafeHttpRequest::WasRedirected() {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (!IsValidResponse())
    return false;
  return request_info_.response.was_redirected;
}

bool SafeHttpRequest::GetFinalUrl(std::string16 *full_url) {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (!IsValidResponse())
    return false;
  *full_url = request_info_.response.final_url;
  return true;
}

bool SafeHttpRequest::GetInitialUrl(std::string16 *full_url) {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (!IsValidResponse())
    return false;
  *full_url = request_info_.full_url;
  return true;
}

bool SafeHttpRequest::SetRequestHeader(const char16 *name,
                                       const char16 *value) {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (GetState() != OPEN)
    return false;
  request_info_.headers.push_back(std::make_pair(name, value));
  return true;
}

bool SafeHttpRequest::Send() {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  return SendImpl();
}

#ifndef OFFICIAL_BUILD
bool SafeHttpRequest::SendBlob(BlobInterface *blob) {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (was_sent_) return false;
  request_info_.post_data_blob = blob;
  return SendImpl();
}
#endif // OFFICIAL_BUILD

bool SafeHttpRequest::SendString(const char16 *name) {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (was_sent_) return false;
  request_info_.post_data_string = name;
  return SendImpl();
}

bool SafeHttpRequest::Abort() {
  assert(IsApartmentThread());

  if (!was_aborted_) {
    MutexLock locker(&request_info_lock_);
    was_aborted_ = true;
    CallAbortOnSafeThread();

    if (GetState() != COMPLETE) {
      request_info_.ready_state = COMPLETE;
      if (listener_) {
        request_info_lock_.Unlock();
        listener_->ReadyStateChanged(this);
        request_info_lock_.Lock();
      }
    }
  }

  return true;
}

bool SafeHttpRequest::GetReadyState(ReadyState *state) {
  assert(IsApartmentThread());
  *state = request_info_.ready_state;
  return true;
}

HttpRequest::CachingBehavior SafeHttpRequest::GetCachingBehavior() {
  assert(IsApartmentThread());
  return request_info_.caching_behavior;
}

bool SafeHttpRequest::SetCachingBehavior(
    HttpRequest::CachingBehavior behavior) {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (GetState() >= SENT)
    return false;
  request_info_.caching_behavior = behavior;
  return true;
}

HttpRequest::RedirectBehavior SafeHttpRequest::GetRedirectBehavior() {
  assert(IsApartmentThread());
  return request_info_.redirect_behavior;
}

bool SafeHttpRequest::SetRedirectBehavior(
    HttpRequest::RedirectBehavior behavior) {
  assert(IsApartmentThread());
  MutexLock locker(&request_info_lock_);
  if (GetState() >= SENT)
    return false;
  request_info_.redirect_behavior = behavior;
  return true;
}

bool SafeHttpRequest::SetListener(HttpRequest::HttpListener *listener,
                                  bool enable_data_available) {
  assert(IsApartmentThread());
  listener_ = listener;
  listener_data_available_enabled_ = enable_data_available;
  return true;
}


//------------------------------------------------------------------------------
// Trivial CallFooOnBarThread methods.
//------------------------------------------------------------------------------

bool SafeHttpRequest::CallSendOnSafeThread() {
  CallAsync(safe_thread_id_, &SafeHttpRequest::OnSendCall);
  return true;
}

bool SafeHttpRequest::CallAbortOnSafeThread() {
  if (IsSafeThread()) {
    OnAbortCall();
  } else {
    CallAsync(safe_thread_id_, &SafeHttpRequest::OnAbortCall);
  }
  return true;
}

bool SafeHttpRequest::CallReadyStateChangedOnApartmentThread() {
  CallAsync(apartment_thread_id_, &SafeHttpRequest::OnReadyStateChangedCall);
  return true;
}

bool SafeHttpRequest::CallDataAvailableOnApartmentThread() {
  CallAsync(apartment_thread_id_, &SafeHttpRequest::OnDataAvailableCall);
  return true;
}


//------------------------------------------------------------------------------
// Async callbacks - OnFooCall methods.
//------------------------------------------------------------------------------

void SafeHttpRequest::OnSendCall() {
  assert(IsSafeThread());
  assert(!native_http_request_);

  MutexLock locker(&request_info_lock_);
  if (was_aborted_) {
    // The request was aborted after this message was sent, ignore it.
    return;
  }

  CreateNativeRequest();
  native_http_request_->SetCachingBehavior(request_info_.caching_behavior);
  native_http_request_->SetRedirectBehavior(request_info_.redirect_behavior);
  bool ok = native_http_request_->Open(request_info_.method.c_str(),
                                       request_info_.full_url.c_str(),
                                       true);  // async
  if (!ok) {
    RemoveNativeRequest();
    request_info_.upcoming_ready_state = COMPLETE;
    CallReadyStateChangedOnApartmentThread();
    return;
  }

  // We defer setting up the listener to skip the OPEN callback
  // since we already called it in SafeHttpRequest::Open
  native_http_request_->SetListener(this, listener_data_available_enabled_);

  for (size_t i = 0; ok && i < request_info_.headers.size(); ++i) {
    ok = native_http_request_->SetRequestHeader(
            request_info_.headers[i].first.c_str(),
            request_info_.headers[i].second.c_str());
  }

  if (ok) {
    // Unlock prior to calling Send as our readystatechanged callback
    // may get called prior to return. This is safe because post_data
    // members are not modified after was_sent_ is set.
    assert(was_sent_);
    request_info_lock_.Unlock();

    if (!request_info_.post_data_string.empty()) {
      ok = native_http_request_->SendString(
          request_info_.post_data_string.c_str());
#ifndef OFFICIAL_BUILD
    } else if (request_info_.post_data_blob.get()) {
      ok = native_http_request_->SendBlob(
          request_info_.post_data_blob.get());
#endif
    } else {
      ok = native_http_request_->Send();
    }

    // Relock so the MutexLock on the stack is balanced
    request_info_lock_.Lock();  
  }
  
  if (!ok) {
    RemoveNativeRequest();
    request_info_.upcoming_ready_state = COMPLETE;
    CallReadyStateChangedOnApartmentThread();
    return;
  }
}

void SafeHttpRequest::OnAbortCall() {
  assert(IsSafeThread());
  if (native_http_request_) {
    // No need to lock here, native_http_request is only accessed on the safe
    // thread.  Furthermore, there should not be a lock here; depending on
    // whether or not the Apartment thread is also the safe thread, the lock
    // may already be held, and our locks are not recursive.
    native_http_request_->SetListener(NULL, false);
    native_http_request_->Abort();
    RemoveNativeRequest();
  }
}

void SafeHttpRequest::OnReadyStateChangedCall() {
  assert(IsApartmentThread());
  {
    MutexLock locker(&request_info_lock_);
    if (was_aborted_) {
      // The request was aborted after this message was sent, ignore it.
      return;
    }
    if (request_info_.upcoming_ready_state < request_info_.ready_state) {
      // We've already signalled this ready state
      return;
    }
    request_info_.ready_state = request_info_.upcoming_ready_state;
  }
  if (listener_) {
    listener_->ReadyStateChanged(this);
  }
}

void SafeHttpRequest::OnDataAvailableCall() {
  assert(IsApartmentThread());
  if (was_aborted_) {
    // The request was aborted after this message was sent, ignore it.
    return;
  }
  if (listener_ && listener_data_available_enabled_) {
    listener_->DataAvailable(this);
  }
}


//------------------------------------------------------------------------------
// Async ping-pong support.
//------------------------------------------------------------------------------

class SafeHttpRequest::AsyncMethodCall : public AsyncFunctor {
public:
  AsyncMethodCall(SafeHttpRequest *request, SafeHttpRequest::Method method)
      : request_(request), method_(method) {
  }

  virtual void Run() {
    (request_.get()->*method_)();
  }

private:
  scoped_refptr<SafeHttpRequest> request_;
  SafeHttpRequest::Method method_;
};


// CallAsync - Posts a message to another thread's event queue, upon delivery
// the method will be invoked on that thread.
void SafeHttpRequest::CallAsync(ThreadId thread_id, 
                                SafeHttpRequest::Method method) {
  AsyncRouter::GetInstance()->CallAsync(thread_id,
      new SafeHttpRequest::AsyncMethodCall(this, method));
}

//------------------------------------------------------------------------------
// HttpRequest::HttpListener implementation.
//------------------------------------------------------------------------------

void SafeHttpRequest::DataAvailable(HttpRequest *source) {
  assert(IsSafeThread());
  assert(source == native_http_request_.get());
  MutexLock locker(&request_info_lock_);
  if (was_aborted_) {
    // The request we're processing has been aborted, but we have not yet
    // received the OnAbort message. We pre-emptively call abort here.
    // When the message does arrive, it will be ignored.
    OnAbortCall();
    return;
  }

  if (was_response_text_accessed_ || !was_data_available_called_) {
    // We don't know if your caller is going to try to read the response
    // text incrementally or not. Here we try to decode and copy the response
    // only if needed. On the first call to DataAvailable, we do so in case our
    // client will access it. On subsequent calls, only do so if the caller
    // has been accessing it.  
    source->GetResponseBodyAsText(&request_info_.response.text);
    was_data_available_called_ = true;
  }
  CallDataAvailableOnApartmentThread();
}

void SafeHttpRequest::ReadyStateChanged(HttpRequest *source) {
  assert(IsSafeThread());
  assert(source == native_http_request_.get());
  scoped_refptr<SafeHttpRequest> reference(this);
  // The extra scope is to ensure we unlock prior to reference falling out
  // of scope.  Strictly speaking, this isn't necessary - the compiler should
  // destroy objects back-to-front, but it's just to make sure.
  {
    ReadyState state;
    source->GetReadyState(&state);

    MutexLock locker(&request_info_lock_);
    if (was_aborted_) {
      // The request we're processing has been aborted, but we have not yet
      // received the OnAbort message. We pre-emptively call abort here, when
      // the message does arrive, it will be ignored.
      OnAbortCall();
      return;
    }

    ReadyState previous_state = request_info_.upcoming_ready_state;

    if (state > previous_state) {
      request_info_.upcoming_ready_state = state;
      if (state >= INTERACTIVE && previous_state < INTERACTIVE) {
        // For HEAD requests, we skip INTERACTIVE and jump straight to COMPLETE
        source->GetAllResponseHeaders(&request_info_.response.headers);
        source->GetStatus(&request_info_.response.status);
        source->GetStatusText(&request_info_.response.status_text);
        source->GetStatusLine(&request_info_.response.status_line);
        source->GetFinalUrl(&request_info_.response.final_url);
        request_info_.response.was_redirected = source->WasRedirected();
      }
      if (state == COMPLETE) {
        source->GetResponseBodyAsText(&request_info_.response.text);
        request_info_.response.body.reset(source->GetResponseBody());
        RemoveNativeRequest();
      }
      CallReadyStateChangedOnApartmentThread();
    }
  }
}


//------------------------------------------------------------------------------
// Other private member functions.
//
// ReadyState accessors should only be called whilst holding the
// request_info_lock_.
//------------------------------------------------------------------------------

// SafeHttpRequest::GetReadyState always returns true.  This method
// is simply for convenience.
HttpRequest::ReadyState SafeHttpRequest::GetState() {
  assert(IsApartmentThread());
  HttpRequest::ReadyState state;
  bool success = GetReadyState(&state);
  assert(success);
  return success ? state : UNINITIALIZED;
}

bool SafeHttpRequest::IsValidResponse() {
  ReadyState state = GetState();
  return (!was_aborted_ && (state == INTERACTIVE || state == COMPLETE)) &&
      (::IsValidResponseCode(request_info_.response.status));
}


bool SafeHttpRequest::SendImpl() {
  assert(IsApartmentThread());
  if (was_sent_ || GetState() != OPEN)
    return false;
  was_sent_ = true;
  if (!CallSendOnSafeThread()) {
    return false;
  }
  return true;
}

void SafeHttpRequest::CreateNativeRequest() {
  assert(IsSafeThread());
  RemoveNativeRequest();
  HttpRequest::Create(&native_http_request_);
  Ref();
}

void SafeHttpRequest::RemoveNativeRequest() {
  assert(IsSafeThread());
  if (native_http_request_) {
    native_http_request_->SetListener(NULL, false);
    native_http_request_.reset(NULL);
    Unref();
  }
}