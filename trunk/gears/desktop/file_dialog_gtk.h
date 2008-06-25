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

#ifndef GEARS_DESKTOP_FILE_DIALOG_GTK_H__
#define GEARS_DESKTOP_FILE_DIALOG_GTK_H__

#if defined(LINUX) && !defined(OS_MACOSX)

#include "gears/desktop/file_dialog.h"

#include <gtk/gtkwindow.h>

class FileDialogGtk : public FileDialog {
 public:
  // Parameters:
  //  parent - The parent window. May be NULL.
  //  multiselect - The user may select multiple files vs one.
  FileDialogGtk(GtkWindow* parent, bool multiselect);
  virtual ~FileDialogGtk();

  virtual bool OpenDialog(const std::vector<Filter>& filters,
                          std::vector<std::string16>* selected_files,
                          std::string16* error);

 private:
  GtkWindow* parent_;
  bool multiselect_;

  DISALLOW_EVIL_CONSTRUCTORS(FileDialogGtk);
};

#endif  // defined(LINUX) && !defined(OS_MACOSX)

#endif  // GEARS_DESKTOP_FILE_DIALOG_GTK_H__
