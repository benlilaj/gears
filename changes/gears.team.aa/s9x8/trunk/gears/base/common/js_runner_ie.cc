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
//
// In IE, JsRunner is a wrapper and most work happens in
// JsRunnerImpl.  The split is necessary so we can do two things:
// (1) provide a simple NewJsRunner/delete interface (not ref-counting), plus
// (2) derive from ATL for the internal implementation (e.g. for ScriptSiteImpl)
//
// JAVASCRIPT ENGINE DETAILS: Internet Explorer uses the JScript engine.
// The interface is IActiveScript, a shared scripting engine interface.
// Communication from the engine to external objects, and communication
// from externally into the engine, is all handled via IDispatch pointers.

#include <assert.h>
#include <map>
#include "gears/base/common/js_runner.h"
#include "gears/base/common/common.h" // for DISALLOW_EVIL_CONSTRUCTORS
#include "gears/base/ie/atl_headers.h"
#include "gears/third_party/AtlActiveScriptSite.h"


class ATL_NO_VTABLE JsRunnerImpl
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IDispatchImpl<IDispatch>,
      public IActiveScriptSiteImpl,
      public IInternetHostSecurityManager,
      public IServiceProviderImpl<JsRunnerImpl>,
      public JsRunnerInterface {
 public:
  BEGIN_COM_MAP(JsRunnerImpl)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IActiveScriptSite)
    COM_INTERFACE_ENTRY(IInternetHostSecurityManager)
    COM_INTERFACE_ENTRY(IServiceProvider)
  END_COM_MAP()

  // For IServiceProviderImpl (used to disable ActiveX objects, along with
  // IInternetHostSecurityManager).
  BEGIN_SERVICE_MAP(JsRunnerImpl)
    SERVICE_ENTRY(SID_SInternetHostSecurityManager)
  END_SERVICE_MAP()

  JsRunnerImpl();
  ~JsRunnerImpl();

  // JsRunnerInterface implementation
  bool AddGlobal(const char16 *name, IGeneric *object);
  bool Start(const char16 *full_script);
  const char16 * GetLastScriptError();

  // IActiveScriptSiteImpl overrides
  STDMETHOD(LookupNamedItem)(const OLECHAR *name, IUnknown **item); 
  STDMETHOD(HandleScriptError)(EXCEPINFO *ei, ULONG line, LONG pos, BSTR src);

  // Implement IInternetHostSecurityManager. We use this to prevent the script
  // engine from creating ActiveX objects, using Java, using scriptlets and
  // various other questionable activities.
  STDMETHOD(GetSecurityId)(BYTE *securityId, DWORD *securityIdSize,
                           DWORD_PTR reserved);
  STDMETHOD(ProcessUrlAction)(DWORD action, BYTE *policy, DWORD policy_size,
                              BYTE *context, DWORD context_size, DWORD flags,
                              DWORD reserved);
  STDMETHOD(QueryCustomPolicy)(REFGUID guid_key, BYTE **policy,
                               DWORD *policy_size, BYTE *context,
                               DWORD context_size, DWORD reserved);

 private:
  CComPtr<IActiveScript> javascript_engine_;
  std::string16 last_script_error_;

  typedef std::map<std::string16, IGeneric *> NameToObjectMap;
  NameToObjectMap global_name_to_object_;

  bool coinit_succeeded_;

  DISALLOW_EVIL_CONSTRUCTORS(JsRunnerImpl);
};


JsRunnerImpl::JsRunnerImpl() : coinit_succeeded_(false) {
}


JsRunnerImpl::~JsRunnerImpl() {
  // decrement all refcounts incremented by AddGlobal()
  JsRunnerImpl::NameToObjectMap::const_iterator it;
  const JsRunnerImpl::NameToObjectMap &map = global_name_to_object_;
  for (it = map.begin(); it != map.end(); ++it) {
    it->second->Release();
  }

  if (coinit_succeeded_) {
    CoUninitialize();
  }
}


bool JsRunnerImpl::AddGlobal(const char16 *name, IGeneric *object) {
  if (!object) { return false; }

  object->AddRef();
  global_name_to_object_[name] = object;
  return true;
}


bool JsRunnerImpl::Start(const char16 *full_script) {
  HRESULT hr;

  coinit_succeeded_ = SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));
  if (!coinit_succeeded_) { return false; }
  // CoUninitialize is handled in dtor

  //
  // Instantiate a JavaScript engine
  //

  CLSID id;
  hr = CLSIDFromProgID(L"JScript", &id);
  if (FAILED(hr)) { return false; }

  hr = javascript_engine_.CoCreateInstance(id);
  if (FAILED(hr)) { return false; }

  // Set the engine's site (which the engine queries when it encounters
  // an unknown name).
  hr = javascript_engine_->SetScriptSite(this);
  if (FAILED(hr)) { return false; }


  // Tell the script engine up to use a custom security manager implementation.
  CComQIPtr<IObjectSafety> javascript_engine_safety;
  javascript_engine_safety = javascript_engine_;
  if (!javascript_engine_safety) { return false; }

  hr = javascript_engine_safety->SetInterfaceSafetyOptions(
                                     IID_IDispatch,
                                     INTERFACE_USES_SECURITY_MANAGER,
                                     INTERFACE_USES_SECURITY_MANAGER);
  if (FAILED(hr)) { return false; }


  //
  // Tell the engine about named globals (set earlier via AddGlobal).
  //

  JsRunnerImpl::NameToObjectMap::const_iterator it;
  const JsRunnerImpl::NameToObjectMap &map = global_name_to_object_;
  for (it = map.begin(); it != map.end(); ++it) {
    const std::string16 &name = it->first;
    hr = javascript_engine_->AddNamedItem(name.c_str(), SCRIPTITEM_ISVISIBLE);
    // TODO(cprince): see if need |= SCRIPTITEM_GLOBALMEMBERS
    if (FAILED(hr)) { return false; }
  }


  //
  // Add script code to the engine instance
  //

  CComQIPtr<IActiveScriptParse> javascript_engine_parser;

  javascript_engine_parser = javascript_engine_;
  if (!javascript_engine_parser) { return false; }

  hr = javascript_engine_parser->InitNew();
  if (FAILED(hr)) { return false; }
  // why does ParseScriptText also AddRef the object?
  hr = javascript_engine_parser->ParseScriptText(full_script,
                                                 NULL, NULL, NULL, 0, 0,
                                                 SCRIPTITEM_ISVISIBLE,
                                                 NULL, NULL);
  if (FAILED(hr)) { return false; }

  //
  // Start the engine running
  //

  // TODO(cprince): do we need STARTED *and* CONNECTED? (Does it matter?)
  // CONNECTED "runs the script and blocks until the script is finished"
  hr = javascript_engine_->SetScriptState(SCRIPTSTATE_STARTED);
  if (FAILED(hr)) { return false; }
  hr = javascript_engine_->SetScriptState(SCRIPTSTATE_CONNECTED);
  if (FAILED(hr)) { return false; }

  // NOTE: at this point, the JS code has returned, and it should have set
  // an onmessage handler.  (If it didn't, the worker is most likely cut off
  // from other workers.  There are ways to prevent this, but they are poor.)

  return true; // succeeded
}


const char16 * JsRunnerImpl::GetLastScriptError() {
  return last_script_error_.c_str();
}



STDMETHODIMP JsRunnerImpl::LookupNamedItem(const OLECHAR *name,
                                           IUnknown **item) {
  IGeneric *found_item = global_name_to_object_[name];
  if (!found_item) { return TYPE_E_ELEMENTNOTFOUND; }

  *item = found_item;
  return S_OK;
}


STDMETHODIMP JsRunnerImpl::HandleScriptError(EXCEPINFO *ei, ULONG line,
                                             LONG pos, BSTR src) {
  last_script_error_ = ei->bstrDescription;
  // TODO(cprince): think about how to get 'line' and 'pos' into the string16.
  // It's not a high priority, because AtlActiveScriptSite can fail to get these
  // values, and because many of the apps we've seen build-up the JS worker
  // code from snippets, making 'line' and 'pos' meaningless.  Fortunately,
  // 'bstrDescription' is available and is the most useful.
  return E_FAIL; // returning success here would hide SetScriptState failures
}


STDMETHODIMP JsRunnerImpl::ProcessUrlAction(DWORD action, BYTE *policy,
                                            DWORD policy_size, BYTE *context, 
                                            DWORD context_size, DWORD flags,
                                            DWORD reserved) {
  // There are many different values of action that could conceivably be
  // asked about: http://msdn2.microsoft.com/en-us/library/ms537178.aspx.
  // Many of them probably don't apply to the scripting host alone, but there
  // is no documentation on which are used, so we just say no to everything to
  // be paranoid. We can whitelist things back if we find that necessary.
  //
  // TODO(aa): Consider whitelisting XMLHttpRequest. IE7 now has a global
  // XMLHttpRequest object, like Safari and Mozilla. I don't believe this
  // object "counts" as an ActiveX Control. If so, it seems like whitelisting
  // the ActiveX version might not matter. In any case, doing this would
  // require figuring out the parallel thing for Mozilla and figuring out how
  // to get XMLHttpRequest the context it needs to make decisions about same-
  // origin.
  *policy = URLPOLICY_DISALLOW;

  // MSDN says to return S_FALSE if you were successful, but don't want to
  // allow the action: http://msdn2.microsoft.com/en-us/library/ms537151.aspx.
  return S_FALSE;
}


STDMETHODIMP JsRunnerImpl::QueryCustomPolicy(REFGUID guid_key, BYTE **policy,
                                             DWORD *policy_size, BYTE *context,
                                             DWORD context_size,
                                             DWORD reserved) {
  // This method is only used when ProcessUrlAction allows ActiveXObjects to be
  // created. Since we always say no, we do not need to implement this method.
  return E_NOTIMPL;
}


STDMETHODIMP JsRunnerImpl::GetSecurityId(BYTE *security_id,
                                         DWORD *security_id_size,
                                         DWORD_PTR reserved) {
  // Again, not used in the configuration we use.
  return E_NOTIMPL;
}


// A wrapper class to AddRef/Release the internal COM object,
// while exposing a simpler new/delete interface to users.
class JsRunner : public JsRunnerInterface {
 public:
  JsRunner() {
    HRESULT hr = CComObject<JsRunnerImpl>::CreateInstance(&com_obj_);
    if (com_obj_) {
      com_obj_->AddRef(); // MSDN says call AddRef after CreateInstance
    }
  }
  ~JsRunner() {
    if (com_obj_) { com_obj_->Release(); }
  }

  bool JsRunner::AddGlobal(const char16 *name, IGeneric *object) {
    return com_obj_->AddGlobal(name, object);
  }
  bool JsRunner::Start(const char16 *full_script) {
    return com_obj_->Start(full_script);
  }
  const char16 * JsRunner::GetLastScriptError() {
    return com_obj_->GetLastScriptError();
  }

 private:
  CComObject<JsRunnerImpl> *com_obj_;

  DISALLOW_EVIL_CONSTRUCTORS(JsRunner);
};


JsRunnerInterface* NewJsRunner() {
  return static_cast<JsRunnerInterface*>(new JsRunner());
}
