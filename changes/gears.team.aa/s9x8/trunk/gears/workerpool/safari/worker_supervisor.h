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

// Keeps track of GearsWorkerPools and unique identifiers.  When a new 
// GearsWorkerPool object is requested in the browser, it will request a
// unique identifier from an instance of this class and also register itself to
// be "supervised" by the GearsWorkerSupervisor instance.

#import <Foundation/Foundation.h>

@class GearsWorkerPool;

// See description above
@interface GearsWorkerSupervisor : NSObject {
 @private
  NSMutableSet *pools_;         // Set of known GearsWorkerPools (STRONG)
  unsigned int nextIdentifier_; // Value of next available identifier
}

//------------------------------------------------------------------------------
// Public
//------------------------------------------------------------------------------
- (NSNumber *)uniqueIdentifier;

- (NSNumber *)superviseWorkerPool:(GearsWorkerPool *)pool;
- (void)unsuperviseWorkerPool:(GearsWorkerPool*)pool;

- (GearsWorkerPool *)workerPoolContainingIdentifier:(NSNumber *)ident;

@end
