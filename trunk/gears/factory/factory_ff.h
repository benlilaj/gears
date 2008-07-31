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

#ifndef GEARS_FACTORY_FACTORY_FF_H__
#define GEARS_FACTORY_FACTORY_FF_H__

#include "genfiles/base_interface_ff.h"
#include "gears/base/common/base_class.h"
#include "gears/base/common/common.h"
#include "gears/factory/factory_impl.h"


// XPConnect names, which hook up XPCOM things with the JavaScript engine.
// These names are used by gears/base/firefox/module.cc.
extern const char *kGearsFactoryContractId;
extern const char *kGearsFactoryClassName;
extern const nsCID kGearsFactoryClassId;


// This is just a thin XPCOM wrapper around a Dispatcher-backed
// GearsFactoryImpl instance.
class GearsFactory : public GearsFactoryInterface,
                     public JsEventHandlerInterface {
 public:
  NS_DECL_ISUPPORTS

  GearsFactory() {}

  NS_IMETHOD InitFactoryFromDOM();

  NS_IMETHOD Create(nsISupports **retval);
  NS_IMETHOD GetBuildInfo(nsAString &retval);
  NS_IMETHOD GetHasPermission(PRBool *retval);
  NS_IMETHOD GetPermission(PRBool *retval);
  NS_IMETHOD GetVersion(nsAString &retval);

 private:
  // This is the callback used to handle the 'JSEVENT_UNLOAD' event.
  virtual void HandleEvent(JsEventType event_type);

  scoped_refptr<GearsFactoryImpl> factory_impl_;
  scoped_ptr<JsEventMonitor> unload_monitor_;

  NS_IMETHOD DelegateToFactoryImpl(const char *name, bool is_property);

  DISALLOW_EVIL_CONSTRUCTORS(GearsFactory);
};


#endif // GEARS_FACTORY_FACTORY_FF_H__