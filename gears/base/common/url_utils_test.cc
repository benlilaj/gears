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

#include "gears/base/common/url_utils.h"

#include "gears/base/common/common.h"  // only for LOG()


bool TestUrlUtils() {
#undef TEST_ASSERT
#define TEST_ASSERT(b) \
{ \
  if (!(b)) { \
    LOG(("TestUrlUtils - failed (%d)\n", __LINE__)); \
    return false; \
  } \
}

  // URLs that begin with a possibly-valid scheme should be non-relative.

  TEST_ASSERT(!IsRelativeUrl(STRING16(L"http:")));
  TEST_ASSERT(!IsRelativeUrl(STRING16(L"HTTP:")));
  TEST_ASSERT(!IsRelativeUrl(STRING16(L"hTTp:")));

  TEST_ASSERT(!IsRelativeUrl(STRING16(L"funkyABCDEFGHIJKLMNOPQRSTUVWXYZ:")));
  TEST_ASSERT(!IsRelativeUrl(STRING16(L"funkyabcdefghijklmnopqrstuvwxyz:")));
  TEST_ASSERT(!IsRelativeUrl(STRING16(L"funky0123456789:")));
  TEST_ASSERT(!IsRelativeUrl(STRING16(L"funky+.-:")));

  TEST_ASSERT(!IsRelativeUrl(STRING16(L"http://www.example.com/foo.txt")));


  // All other URLs should be relative.

  TEST_ASSERT(IsRelativeUrl(STRING16(L"foo.txt")));
  TEST_ASSERT(IsRelativeUrl(STRING16(L"foo.txt?http://evil.com")));
  TEST_ASSERT(IsRelativeUrl(STRING16(L"/foo.txt")));

  TEST_ASSERT(IsRelativeUrl(STRING16(L"http")));  // no trailing colon
  TEST_ASSERT(IsRelativeUrl(STRING16(L"0http:")));  // no leading alpha
  TEST_ASSERT(IsRelativeUrl(STRING16(L":")));


  // ResolveAndNormalize test cases

  const char16 *base_page =
                   STRING16(L"http://server/directory/page");
  const char16 *base_page_with_hash =
                   STRING16(L"http://server/directory/page#bar");
  const char16 *base_page_with_query =
                   STRING16(L"http://server/directory/page?query");
  const char16 *base_dir =
                   STRING16(L"http://server/directory/");
  const char16 *base_dir_with_hash =
                   STRING16(L"http://server/directory/#bar");
  const char16 *base_dir_with_query =
                   STRING16(L"http://server/directory/?query");

  const struct {
      const char16 *base;
      const char16 *url;
      const char16 *resolved;
  } kCases[] = {
    {
      base_page,
      STRING16(L"foo"),
      STRING16(L"http://server/directory/foo")
    },
    {
      base_page_with_hash,
      STRING16(L"foo"),
      STRING16(L"http://server/directory/foo")
    },
    {
      base_page_with_query,
      STRING16(L"foo"),
      STRING16(L"http://server/directory/foo")
    },
    {
      base_dir,
      STRING16(L"foo"),
      STRING16(L"http://server/directory/foo")
    },
    {
      base_dir_with_hash,
      STRING16(L"foo"),
      STRING16(L"http://server/directory/foo")
    },
    {
      base_dir_with_query,
      STRING16(L"foo"),
      STRING16(L"http://server/directory/foo")
    },
    {
      base_page,
      STRING16(L"/foo"),
      STRING16(L"http://server/foo")
    },
    {
      base_page,
      STRING16(L"./foo"),
      STRING16(L"http://server/directory/foo")
    },
    {
      base_page,
      STRING16(L"../foo"),
      STRING16(L"http://server/foo")
    },
    {
      base_page,
      STRING16(L"foo#bar"),
      STRING16(L"http://server/directory/foo")
    },
    {
      base_page,
      STRING16(L"foo?bar"),
      STRING16(L"http://server/directory/foo?bar")
    },
    {
      NULL,
      STRING16(L"http://server/directory/foo#bar"),
      STRING16(L"http://server/directory/foo")
    }
  };

  for (size_t i = 0; i < ARRAYSIZE(kCases); ++i) {
    std::string16 resolved;
    TEST_ASSERT(ResolveAndNormalize(kCases[i].base, kCases[i].url, &resolved));
    TEST_ASSERT(resolved == kCases[i].resolved);
  }

  LOG(("TestUrlUtilsAll - passed\n"));
  return true;
}
