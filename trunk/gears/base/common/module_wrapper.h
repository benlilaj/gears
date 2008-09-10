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

#ifndef GEARS_BASE_COMMON_MODULE_WRAPPER_H__
#define GEARS_BASE_COMMON_MODULE_WRAPPER_H__

// TODO(nigeltao): Delete this file (and all occurences of #include
// "gears/base/common/module_wrapper.h" from the rest of the codebase), push
// the platform-specific #include "gears/base/xx/module_wrapper.h" into
// js_runner_xx.cc, and move CreateModule<T> out of base/xx/module_wrapper.h
// and into base/common/base_class.h.

#ifdef BROWSER_NPAPI
#include "gears/base/npapi/module_wrapper.h"
#elif BROWSER_FF
#include "gears/base/firefox/module_wrapper.h"
#elif BROWSER_IE
#include "gears/base/ie/module_wrapper.h"
#else
// TODO: Add more browser support here.
#endif

// Used to set up a Gears module's wrapper.
#define DECLARE_GEARS_WRAPPER(GearsClass) \
DECLARE_DISPATCHER(GearsClass)

#endif // GEARS_BASE_COMMON_MODULE_WRAPPER_H__
