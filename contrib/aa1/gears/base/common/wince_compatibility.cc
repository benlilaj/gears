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
// Implementation of Windows specific functions that don't exist in Windows
// Mobile 5.

// TODO(andreip): remove platform-specific #ifdef guards when OS-specific
// sources (e.g. WIN32_CPPSRCS) are implemented
#ifdef WINCE
#include "gears/base/common/wince_compatibility.h"

#include <shellapi.h>

#include "gears/base/common/common.h"
#include "gears/base/common/paths.h"
#include "gears/base/common/string_utils.h"

HANDLE CMutexWince::global_mutex_ = NULL;
CriticalSection CMutexWince::lock_;
const char16* kGlobalMutexName =
    STRING16(PRODUCT_SHORT_NAME L"GearsGlobalMutexWince");

// Used by SHCreateDirectoryEx.
static void SkipTokens(const std::string16 &path,
                       int &pos,
                       bool skip_separators);

// GetSystemTimeAsFileTime does not exist on Windows Mobile,
// so we implement it here by getting the system time and
// then converting it to file time.
void GetSystemTimeAsFileTime(LPFILETIME filetime) {
  SYSTEMTIME systemtime;
  GetSystemTime(&systemtime);
  SystemTimeToFileTime(&systemtime, filetime);
}

// There seem to be no way to implement this properly on Windows Mobile
// since the algorithm for path shortening isn't fully specified, according
// to http://en.wikipedia.org/wiki/8.3_filename. Using FindFirstFileA isn't
// an alternative either since that method isn't exported by coredll.lib.
// FindFirstFileW uses a different structure to return the file information
// and that structure is missing exactly the cAlternateFilename field, which
// would have been the one that contained the short name.
DWORD GetShortPathNameW(LPCTSTR path_long,
                        LPTSTR path_short,
                        DWORD path_short_max_size) {
 int long_path_size = wcslen(path_long) + 1;  // +1 for the ending \0
 if (long_path_size <= static_cast<int>(path_short_max_size)) {
   wcsncpy(path_short, path_long, path_short_max_size);
   return long_path_size - 1;
 } else {
   return long_path_size;
 }
}

// This function creates a file system folder whose fully qualified
// path is given by full_dirpath. If one or more of the intermediate folders
// do not exist, they are created as well.
// According to http://msdn2.microsoft.com/en-us/library/aa365247.aspx
// Windows API functions accept both "\" and "/", so we will do the same.
int SHCreateDirectoryEx(HWND window,
                        LPCTSTR full_dirpath,
                        const SECURITY_ATTRIBUTES *security_attributes) {
  std::string16 path(full_dirpath);
  if (!path.length()) return ERROR_BAD_PATHNAME;

  // Traverse the path and create the directories one by one.
  int pos = 0;
  // Skip leading separators.
  SkipTokens(path, pos, true);
  while (pos < static_cast<int>(path.length())) {
    // Find next separator.
    SkipTokens(path, pos, false);
    // Skip next consecutive separators, if any.
    SkipTokens(path, pos, true);
    // Create the directory.
    if (!CreateDirectory(path.substr(0, pos).c_str(), NULL)) {
      DWORD error = GetLastError();
      if (error != ERROR_ALREADY_EXISTS) return error;
    }
  }
  return GetLastError();
}

HRESULT SHGetFolderPath(HWND hwndOwner,
                        int nFolder,
                        HANDLE hToken,
                        DWORD dwFlags,
                        LPTSTR pszPath) {
  return SHGetSpecialFolderPath(hwndOwner, pszPath, nFolder, false);
}

BOOL IsNetworkAlive(LPDWORD lpdwFlags) {
  BOOL alive = false;
  CONNMGR_CONNECTION_DETAILED_STATUS* status_buffer_ptr = NULL;
  DWORD size = 0;
  HRESULT hr = ConnMgrQueryDetailedStatus(status_buffer_ptr, &size);
  if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
    uint8* buffer = new uint8[size];
    status_buffer_ptr = reinterpret_cast<CONNMGR_CONNECTION_DETAILED_STATUS*>
        (buffer);
    ZeroMemory(status_buffer_ptr, size);
    hr = ConnMgrQueryDetailedStatus(status_buffer_ptr, &size);
    if (SUCCEEDED(hr)) {
      while (status_buffer_ptr) {
        if (status_buffer_ptr->dwConnectionStatus == CONNMGR_STATUS_CONNECTED &&
            (status_buffer_ptr->pIPAddr != NULL ||
            status_buffer_ptr->dwType == CM_CONNTYPE_PROXY)) {
          // We conclude that the network is alive if there is one
          // connection in the CONNMGR_STATUS_CONNECTED state and
          // the device has an IP address or it is connected in
          // proxy mode (e.g. ActiveSync).
          alive = true;
          break;
        }
        status_buffer_ptr = status_buffer_ptr->pNext;
      }
    }
    delete [] buffer;
  }
  return alive;
}

BOOL CMutexWince::Open(DWORD dwAccess, BOOL bInheritHandle, LPCTSTR pszName) {  
  // On Windows Mobile we are forced to implement CMutex::Open() using
  // the CreateMutex() win32 API function. This will open an existing mutex
  // or, if one doesn't exist already, will create a new mutex. However,
  // given the semantics of ATL CMutex::Open(), the creation of a new mutex
  // is an unwanted side-effect and we need to hide it from other processes
  // that are simultaneously calling this method. We therefore need to
  // serialize Open method calls using a global mutex. Furthermore,
  // we also need to use a critical section to guard against
  // concurrent initialization of this global mutex by different threads.
  CritSecLock locker(lock_);
  assert(m_h == NULL);
  if (!global_mutex_) {
    global_mutex_ = CreateMutex(NULL, FALSE, kGlobalMutexName);
    if (!global_mutex_) return false;  // Returning early!
  }
  // We now have a handle to the global mutex. We may have created
  // it or may have opened the existing one if another process had
  // already created it. Although we cannot have multiple instances
  // of IE mobile running at the same time, the browser control
  // (together with Gears) may be embedded in some other application.
  BOOL success = false;
  DWORD result = WaitForSingleObject(global_mutex_, INFINITE);
  if (result == WAIT_OBJECT_0) {
    // We have ownership of global_mutex_.
    m_h = CreateMutex(NULL, FALSE, pszName);
    if (m_h) {
      // If m_h is not NULL, GetLastError() can only return
      // ERROR_ALREADY_EXISTS or success.
      if (GetLastError() != ERROR_ALREADY_EXISTS) {
        // We didn't mean to create a mutex here, so let's close it.
        CloseHandle(m_h);
        m_h = NULL;
      } else {
        success = true;
      }
    }
    // Give up ownership of global_mutex_.
    ReleaseMutex(global_mutex_);
  }
  return success;
}

// Internal

static bool IsSeparator(const char16 token) {
  static const char16 kPathSeparatorAlternative = L'/';
  return ((token == kPathSeparator) || (token == kPathSeparatorAlternative));
}

// Skips tokens of the given type (separators or non-separators).
static void SkipTokens(const std::string16 &path,
                       int &pos,
                       bool skip_separators) {
  while (pos < static_cast<int>(path.length()) &&
        (IsSeparator(path[pos]) == skip_separators)) {
    pos++;
  }
}

#endif  // WINCE