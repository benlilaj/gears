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

// Safari implementation of HttpRequest using an NSURLConnection.

#include <iostream>

#include "gears/base/common/atomic_ops.h"
#include "gears/base/common/http_utils.h"
#include "gears/base/common/string_utils.h"
#include "gears/base/common/url_utils.h"
#include "gears/base/npapi/browser_utils.h"
#include "gears/base/safari/cf_string_utils.h"
#include "gears/localserver/common/http_request.h"
#include "gears/localserver/safari/http_request_delegate.h"

// PIMPL store for Objective-C delegate.
struct SFHttpRequest::HttpRequestData {
  HttpRequestDelegate *delegate;
  
  HttpRequestData() : delegate(nil) {}
  
  ~HttpRequestData() {
    [delegate release];
    delegate = NULL;
  }
};

//------------------------------------------------------------------------------
// Create
//------------------------------------------------------------------------------
HttpRequest *HttpRequest::Create() {
  HttpRequest *request = new SFHttpRequest;
  
  request->AddReference();
  
  return request;
}

//------------------------------------------------------------------------------
// AddReference
//------------------------------------------------------------------------------
int SFHttpRequest::AddReference() {
  return AtomicIncrement(&ref_count_, 1);
}

//------------------------------------------------------------------------------
// ReleaseReference
//------------------------------------------------------------------------------
int SFHttpRequest::ReleaseReference() {
  int cnt = AtomicIncrement(&ref_count_, -1);
  
  if (cnt < 1)
    delete this;
  
  return cnt;
}

//------------------------------------------------------------------------------
// Constructor / Destructor
//------------------------------------------------------------------------------
SFHttpRequest::SFHttpRequest()
  : listener_(NULL), ready_state_(UNINITIALIZED), ref_count_(0), 
    caching_behavior_(USE_ALL_CACHES), 
    redirect_behavior_(FOLLOW_ALL),
    was_sent_(false), was_aborted_(false),
    was_redirected_(false), async_(false) {

  delegate_holder_ = new HttpRequestData;
}

SFHttpRequest::~SFHttpRequest() {
  delete delegate_holder_;
}

//------------------------------------------------------------------------------
// GetReadyState
//------------------------------------------------------------------------------
bool SFHttpRequest::GetReadyState(ReadyState *state) {
  *state = ready_state_;
  return true;
}

//------------------------------------------------------------------------------
// GetResponseBodyAsText
//------------------------------------------------------------------------------
bool SFHttpRequest::GetResponseBodyAsText(std::string16 *text) {
  assert(text);
  if (!IsInteractiveOrComplete() || was_aborted_)
    return false;

  return [delegate_holder_->delegate responseAsString:text];
}

//------------------------------------------------------------------------------
// GetResponseBody
//------------------------------------------------------------------------------
bool SFHttpRequest::GetResponseBody(std::vector<uint8> *body) {
  assert(body);
  if (!(IsComplete() && !was_aborted_)) {
    return false;
  }
  
  [delegate_holder_->delegate responseBytes:body];
  return true;
}

//------------------------------------------------------------------------------
// GetResponseBody
//------------------------------------------------------------------------------
std::vector<uint8> *SFHttpRequest::GetResponseBody() {
  scoped_ptr< std::vector<uint8> > body(new std::vector<uint8>);
  if (!GetResponseBody(body.get())) {
    return NULL;
  } else {
    return body.release();
  }
}

//------------------------------------------------------------------------------
// GetStatus
//------------------------------------------------------------------------------
bool SFHttpRequest::GetStatus(int *status) {
  if (!(IsInteractiveOrComplete() && !was_aborted_)) {
    return false;
  }
  
  [delegate_holder_->delegate statusCode:status];
  
  return true;
}

//------------------------------------------------------------------------------
// GetStatusText
//------------------------------------------------------------------------------
bool SFHttpRequest::GetStatusText(std::string16 *status_text) {
  if (!(IsInteractiveOrComplete() && !was_aborted_)) {
    return false;
  }

  [delegate_holder_->delegate statusText:status_text];
  return true;
}

//------------------------------------------------------------------------------
// GetStatusLine
//------------------------------------------------------------------------------
bool SFHttpRequest::GetStatusLine(std::string16 *status_line) {
  if (!(IsInteractiveOrComplete() && !was_aborted_)) {
    return false;
  }

  int status_code;
  [delegate_holder_->delegate statusCode:&status_code];
  std::string16 status_text;
  [delegate_holder_->delegate statusText:&status_text];

  std::string16 ret(STRING16(L"HTTP/1.1 "));
  ret += IntegerToString16(status_code);
  ret += STRING16(L" ");
  ret += status_text;

  *status_line = ret;

  return true;
}

//------------------------------------------------------------------------------
// Open
//------------------------------------------------------------------------------
bool SFHttpRequest::Open(const char16 *method, const char16 *url, bool async) {
  assert(!IsRelativeUrl(url));
  // TODO(michaeln): Add some of the sanity checks the IE implementation has.

  // Do not allow reuse of this object for multiple requests.
  assert(!IsComplete());
  
  if (!IsUninitialized()) {
    return false;
  }

  method_ = method;
  url_ = url;
  async_ = async;
  if (!origin_.InitFromUrl(url)) {
    return false;
  }
  
  delegate_holder_->delegate = [[HttpRequestDelegate alloc] initWithOwner:this];
  if (!delegate_holder_->delegate) {
    return false;
  }
  if (![delegate_holder_->delegate open:url method:method]) {
    return false;
  }

  SetReadyState(OPEN);
  return true;
}

//------------------------------------------------------------------------------
// SetRequestHeader
//------------------------------------------------------------------------------
bool SFHttpRequest::SetRequestHeader(const char16* name, const char16* value) {
  if (was_sent_)
    return false;
  
  headers_to_send_.push_back(HttpHeader(name, value));
  return true;
}

//------------------------------------------------------------------------------
// RedirectBehavior
//------------------------------------------------------------------------------

bool SFHttpRequest::WasRedirected() {
  return IsInteractiveOrComplete() && !was_aborted_ && was_redirected_;
}

bool SFHttpRequest::GetFinalUrl(std::string16 *full_url) {
  if (!IsInteractiveOrComplete() || was_aborted_)
    return false;

  if (WasRedirected())
    *full_url = redirect_url_;
  else
    *full_url = url_;
  return true;
}

bool SFHttpRequest::GetInitialUrl(std::string16 *full_url) {
  *full_url = url_;  // may be empty if request has not occurred
  return true;
}

//------------------------------------------------------------------------------
// Send, SendString, SendBlob, SendImpl
//------------------------------------------------------------------------------

bool SFHttpRequest::Send() {
  std::string empty;
  return SendImpl(empty);
}


bool SFHttpRequest::SendString(const char16 *data) {
  if (was_sent_ || !data) {
    return false;
  }
  if (!IsPostOrPut()) {
    return false;
  }

  std::string post_data;
  String16ToUTF8(data, &post_data);
  return SendImpl(post_data);
}


#ifndef OFFICIAL_BUILD
bool SFHttpRequest::SendBlob(BlobInterface *data) {
  // TODO(bpm): implement!
  assert(false);
  return false;
}
#endif  // !OFFICIAL_BUILD


bool SFHttpRequest::SendImpl(const std::string &post_data) {
  if (was_sent_) {
    return false;
  }
  was_sent_ = true;
  
  //If Content-type header not set by the client, we set it to 'text/plain' in
  //order to match behavior of FF implementation.
  bool has_content_type_header = false;
  for (HttpHeaderVectorConstIterator it = headers_to_send_.begin();
       it != headers_to_send_.end(); 
       ++it) {
    const std::string16 &header_name = it->first;
    if (StringCompareIgnoreCase(header_name.c_str(), 
                                HttpConstants::kContentTypeHeader) == 0) {
      has_content_type_header = true;
      break;
    }
  }
  
  if (!has_content_type_header) {
    headers_to_send_.push_back(HttpHeader(HttpConstants::kContentTypeHeader,
                                          HttpConstants::kMimeTextPlain));
  }

  std::string16 user_agent;
  if (!BrowserUtils::GetUserAgentString(&user_agent)) {
    return false;
  }
  
  // TODO(playmobil): Handle case of ShouldBypassLocalServer().
  if (![delegate_holder_->delegate send: post_data 
                             userAgent: user_agent
                               headers: headers_to_send_
                    bypassBrowserCache: ShouldBypassBrowserCache()]) {
     Reset();
     return false;
  }
  
  SetReadyState(SENT);
  
  // Block until completion on synchronous requests.
  if (!async_) {
    // NSURLConnection has a default idle timeout of 60 seconds after which the
    // connection should be terminated, we rely on this to ensure that the
    // loop exits.
    CFTimeInterval ten_milliseconds = 10*10e-3;
    AddReference();
    while (ready_state_ != HttpRequest::COMPLETE && !was_aborted_) {
      CFRunLoopRunInMode(kCFRunLoopDefaultMode, ten_milliseconds, false);
    }
    ReleaseReference();
  }
  
  return true;
}

//------------------------------------------------------------------------------
// GetAllResponseHeaders
//------------------------------------------------------------------------------
bool SFHttpRequest::GetAllResponseHeaders(std::string16 *headers) {
  if (!(IsInteractiveOrComplete() && !was_aborted_)) {
    return false;
  }
  
  HttpHeaderVector response_headers;
  [delegate_holder_->delegate headers:&response_headers];
  std::string16 header_str;
  for (HttpHeaderVectorConstIterator hdr = response_headers.begin();
       hdr != response_headers.end();
       ++hdr) {
    // TODO(playmobil): do we need to fixup the Content-Encoding and
    // Content-Length headers like the FF implementation to get LocalServer
    // to work?       
    if (!hdr->second.empty()) {  // NULL means do not output
      header_str += hdr->first;
      header_str += std::string16(STRING16(L": "));
      header_str += hdr->second;
      header_str += HttpConstants::kCrLf;
    }
  }
  
  header_str += HttpConstants::kCrLf;  // blank line at the end
  *headers = header_str;
  
  return true;
}

//------------------------------------------------------------------------------
// GetResponseHeader
//------------------------------------------------------------------------------
bool SFHttpRequest::GetResponseHeader(const char16 *name,
                                      std::string16 *value) {
  if (!(IsInteractiveOrComplete() && !was_aborted_)) {
    return false;
  }
  
  [delegate_holder_->delegate headerByName:name value:value];
    
  return true;
}

//------------------------------------------------------------------------------
// Abort
//------------------------------------------------------------------------------
bool SFHttpRequest::Abort() {
  [delegate_holder_->delegate abort];
  
  was_aborted_ = true;
  SetReadyState(COMPLETE);
  return true;
}

//------------------------------------------------------------------------------
// SetOnReadyStateChange
//------------------------------------------------------------------------------
bool SFHttpRequest::SetOnReadyStateChange(
                        ReadyStateListener *listener) {
  listener_ = listener;
  return true;
}

//------------------------------------------------------------------------------
// SetReadyState
//------------------------------------------------------------------------------
void SFHttpRequest::SetReadyState(ReadyState state) {
  if (state > ready_state_) {
    ready_state_ = state;
    if (listener_) {
      listener_->ReadyStateChanged(this);
    }
  }
}

//------------------------------------------------------------------------------
// AllowRedirect
//------------------------------------------------------------------------------
bool SFHttpRequest::AllowRedirect(const std::string16 &redirect_url) {
  bool follow = false;
  switch (redirect_behavior_) {
    case FOLLOW_ALL:
      follow = true;
      break;

    case FOLLOW_NONE:
      follow = false;
      break;

    case FOLLOW_WITHIN_ORIGIN:
      follow = origin_.IsSameOriginAsUrl(redirect_url.c_str());
      break;
  }

  if (follow) {
    was_redirected_ = true;
    redirect_url_ = redirect_url;
  }

  return follow;
}

//------------------------------------------------------------------------------
// Reset
//------------------------------------------------------------------------------
void SFHttpRequest::Reset() {
  [delegate_holder_->delegate release];
  delegate_holder_->delegate = nil;
  method_.clear();
  url_.clear();
  caching_behavior_ = USE_ALL_CACHES;
  redirect_behavior_ = FOLLOW_ALL;
  was_sent_ = false;
  was_aborted_ = false;
  was_redirected_ = false;
  async_ = false;
  redirect_url_.clear();
  ready_state_ = UNINITIALIZED;
}
