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

#include <assert.h>
#if defined(WIN32) || defined(WINCE)
#include <windows.h>
#elif defined(LINUX) || defined(OS_MACOSX)
#include <pthread.h>
#include <sched.h>
#else
#error "OS not defined."
#endif

#include "gears/base/common/mutex.h"

Mutex::Mutex()
#ifdef DEBUG
    : is_locked_(false)
#endif
{
#if defined(WIN32) || defined(WINCE)
  InitializeCriticalSection(&crit_sec_);
#elif defined(LINUX) || defined(OS_MACOSX)
  pthread_mutex_init(&mutex_, NULL);
#endif
}


Mutex::~Mutex() {
#if defined(WIN32) || defined(WINCE)
  DeleteCriticalSection(&crit_sec_);
#elif defined(LINUX) || defined(OS_MACOSX)
  pthread_mutex_destroy(&mutex_);
#endif
}


void Mutex::Lock() {
#if defined(WIN32) || defined(WINCE)
  EnterCriticalSection(&crit_sec_);
#elif defined(LINUX) || defined(OS_MACOSX)
  pthread_mutex_lock(&mutex_);
#endif

#ifdef DEBUG
  assert(!is_locked_); // Google frowns upon mutex re-entrancy
  is_locked_ = true;
#endif
}


void Mutex::Unlock() {
#ifdef DEBUG
  assert(is_locked_);
  is_locked_ = false;
#endif

#if defined(WIN32) || defined(WINCE)
  LeaveCriticalSection(&crit_sec_);
#elif defined(LINUX) || defined(OS_MACOSX)
  pthread_mutex_unlock(&mutex_);
#endif
}


void Mutex::Await(const Condition &cond) {
  while (!cond.Eval()) {
    Unlock();

    // Yield the rest of our CPU timeslice before reacquiring the lock.
    // Otherwise we'll spin pointlessly here, hurting performance.
    // (The Condition cannot possibly change when no other thread runs.)
#if defined(WIN32) || defined(WINCE)
    Sleep(0);                       // equivalent to 'yield' in Win32
#elif defined(LINUX) || defined(OS_MACOSX)
    sched_yield();
#endif

    Lock();
  }
}