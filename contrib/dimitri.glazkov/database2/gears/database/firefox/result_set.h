// Copyright 2005, Google Inc.
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

#ifndef GEARS_DATABASE_FIREFOX_RESULT_SET_H__
#define GEARS_DATABASE_FIREFOX_RESULT_SET_H__

#include <gecko_sdk/include/nsComponentManagerUtils.h>
#include <gecko_internal/nsIVariant.h>

#include "genfiles/database.h"
#include "gears/base/common/base_class.h"
#include "gears/base/common/common.h"


// Object identifiers
extern const char *kGearsResultSetClassName;
extern const nsCID kGearsResultSetClassId;


struct sqlite3_stmt;
class GearsDatabase;

class GearsResultSet
    : public ModuleImplBaseClass,
      public GearsResultSetInterface {
 public:
  NS_DECL_ISUPPORTS
  GEARS_IMPL_BASECLASS
  // End boilerplate code. Begin interface.

  GearsResultSet();

  NS_IMETHOD Field(PRInt32 index, nsIVariant **retval);
  NS_IMETHOD FieldByName(const nsAString &field_name, nsIVariant **retval);
  NS_IMETHOD FieldName(PRInt32 index, nsAString &retval);
  NS_IMETHOD FieldCount(PRInt32 *retval);
  NS_IMETHOD Close(void);
  NS_IMETHOD Next(void);
  NS_IMETHOD IsValidRow(PRBool *retval);

 protected:
  ~GearsResultSet();

 private:
  friend class GearsDatabase;

  // Helper called by GearsDatabase.execute to initialize the result set
  bool InitializeResultSet(sqlite3_stmt *statement,
                           GearsDatabase *db,
                           std::string16 *error_message);

  // Helper shared by Next() and SetStatement()
  bool NextImpl(std::string16 *error_message);
  bool Finalize();

  GearsDatabase *database_;
  sqlite3_stmt *statement_;
  bool is_valid_row_;

  DISALLOW_EVIL_CONSTRUCTORS(GearsResultSet);
};

#endif // GEARS_DATABASE_FIREFOX_RESULT_SET_H__