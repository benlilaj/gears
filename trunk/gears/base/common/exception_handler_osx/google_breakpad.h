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
//
// Wrapper around Breakpad open-source project to provide a simple C API to crash
// reporting for Google applications.  By default, if any machine-level
// exception (e.g., EXC_BAD_ACCESS) occurs, it will be handled by the
// GoogleBreakpadRef object as follows: 
// - Create a minidump file (see breakpad for details) located in ~/Logs/Google
//   with a unique name containing your product name.
// - Throttle the posting of this 
// - Prompt the user (using CFUserNotification) 
// - Invoke a command line reporting tool to send the minidump to the standard
//   Google crash reporting server.
//
// By specifying parameters to the GoogleBreakpadCreate function, you can modify
// the default behavior to suit your needs and wants and desires.

#ifndef GEARS_BASE_COMMON_EXCEPTION_HANDLER_OSX_GOOGLE_BREAKPAD_H__
#define GEARS_BASE_COMMON_EXCEPTION_HANDLER_OSX_GOOGLE_BREAKPAD_H__

typedef void *GoogleBreakpadRef;

#ifdef __cplusplus
extern "C" {
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>

  // Keys for configuration file
#define kReporterMinidumpDirectoryKey "MinidumpDir"
#define kReporterMinidumpIDKey        "MinidumpID"
  
// Specify some special keys to be used in the configuration file that is
// generated by the GoogleBreakpad and consumed by the Reporter.
#define GOOGLE_BREAKPAD_PRODUCT_DISPLAY       "GoogleBreakpadProductDisplay"
#define GOOGLE_BREAKPAD_PRODUCT               "GoogleBreakpadProduct"
#define GOOGLE_BREAKPAD_VERSION               "GoogleBreakpadVersion"
#define GOOGLE_BREAKPAD_URL                   "GoogleBreakpadURL"
#define GOOGLE_BREAKPAD_REPORT_INTERVAL       "GoogleBreakpadReportInterval"
#define GOOGLE_BREAKPAD_SKIP_CONFIRM          "GoogleBreakpadSkipConfirm"
#define GOOGLE_BREAKPAD_SEND_AND_EXIT         "GoogleBreakpadSendAndExit"
#define GOOGLE_BREAKPAD_INSPECTOR_LOCATION    "GoogleBreakpadInspectorLocation"
#define GOOGLE_BREAKPAD_REPORTER_EXE_LOCATION "GoogleBreakpadReporterExeLocation"  

// Create a new GoogleBreakpadRef object and install it as an exception handler.
// The |parameters| will typically be the contents of your bundle's Info.plist.
//
// You can also specify these additional keys for customizable behavior:
// Key:                           Value:
// GOOGLE_BREAKPAD_PRODUCT        Product name (e.g., Google_Gears)
// GOOGLE_BREAKPAD_VERSION        Product version (e.g., 1.2.3)
// GOOGLE_BREAKPAD_URL            URL destination for reporting
// GOOGLE_BREAKPAD_REPORT_INTERVAL  # of seconds between sending reports.  If an
//                                additional report is generated within this
//                                time, it will be ignored.  Default: 3600sec.
//                                Specify 0 to send all reports.
// GOOGLE_BREAKPAD_SKIP_CONFIRM   If true, the reporter will send the report
//                                without any user intervention.
// GOOGLE_BREAKPAD_SEND_AND_EXIT  If true, the handler will exit after sending.
//                                This will prevent any other handler (e.g.,
//                                CrashReporter) from getting the crash.
// GOOGLE_BREAKPAD_INSPECTOR_LOCATION The full path to the inspector executable.
// GOOGLE_BREAKPAD_REPORTER_EXE_LOCATION The full path to the reporter 
//                                       executable.
// 
// The GOOGLE_BREAKPAD_PRODUCT and GOOGLE_BREAKPAD_VERSION are required to have non-
// NULL values.  By default, the GOOGLE_BREAKPAD_PRODUCT will be the CFBundleName
// and the GOOGLE_BREAKPAD_VERSION will be the CFBundleVersion when provided
// these keys from the bundle Info.plist.  If the GOOGLE_BREAKPAD_PRODUCT or
// GOOGLE_BREAKPAD_VERSION are ultimately undefined, GoogleBreakpadCreate() will
// fail.  You have been warned.
//
// If you are running in a debugger, breakpad will not install, unless the
// GOOGLE_BREAKPAD_IGNORE_DEBUGGER envionment variable is set and/or non-zero.
//
// The GOOGLE_BREAKPAD_SKIP_CONFIRM and GOOGLE_BREAKPAD_SEND_AND_EXIT default
// values are NO and YES.  However, they can be controlled by setting their
// values in a user or global plist.
//
// It's easiest to use Breakpad via the Framework, but if you're compiling the
// code in directly, GOOGLE_BREAKPAD_INSPECTOR_LOCATION and 
// GOOGLE_BREAKPAD_REPORTER_EXE_LOCATION allow you to specify custom paths
// to the helper executables.
//
  
// Returns a new GoogleBreakpadRef object on success, NULL otherwise.
GoogleBreakpadRef GoogleBreakpadCreate(NSDictionary *parameters);

// Uninstall and release the data associated with |ref|.
void GoogleBreakpadRelease(GoogleBreakpadRef ref);

// User defined key and value string storage
// All set keys will be uploaded with the minidump if a crash occurs
// Keys and Values are limited to 255 bytes (256 - 1 for terminator).
// NB this is BYTES not GLYPHS.
// Anything longer than 255 bytes will be truncated. Note that the string is
// converted to UTF8 before truncation, so any multibyte character that
// straddles the 255 byte limit will be mangled.
//
// A maximum number of 64 key/value pairs are supported.  An assert() will fire
// if more than this number are set.
void GoogleBreakpadSetKeyValue(GoogleBreakpadRef ref, NSString *key, NSString *value);
NSString *GoogleBreakpadKeyValue(GoogleBreakpadRef ref, NSString *key);
void GoogleBreakpadRemoveKeyValue(GoogleBreakpadRef ref, NSString *key);

// Generate a minidump and send
void GoogleBreakpadGenerateAndSendReport(GoogleBreakpadRef ref);

#ifdef __cplusplus
}
#endif
#endif  // GEARS_BASE_COMMON_EXCEPTION_HANDLER_OSX_GOOGLE_BREAKPAD_H__
