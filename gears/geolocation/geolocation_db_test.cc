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

#if USING_CCTESTS

#include "gears/geolocation/geolocation_db_test.h"

#include "gears/geolocation/geolocation_db.h"

// Local function.
// Checks that two position objects are equal.
static bool ArePositionsEqual(const Position &left, const Position &right);

static const char16 *kTestPositionString = STRING16(L"test position");

bool TestGeolocationDB(std::string16 *error) {
  assert(error);

  GeolocationDB *db = GeolocationDB::GetDB();
  if (!db) {
    *error += STRING16(L"TestGeolocationDB(): Failed to get Geolocation DB. ");
    return false;
  }

  Position position;
  position.latitude              = 1.0;
  position.longitude             = 2.0;
  position.altitude              = 3;
  position.horizontal_accuracy   = 4;
  position.vertical_accuracy     = 5;
  position.timestamp             = 6;
  position.error                 = STRING16(L"error");
  position.address.street_number = STRING16(L"street number");
  position.address.street        = STRING16(L"street");
  position.address.premises      = STRING16(L"premises");
  position.address.city          = STRING16(L"city");
  position.address.county        = STRING16(L"county");
  position.address.region        = STRING16(L"region");
  position.address.country       = STRING16(L"country");
  position.address.country_code  = STRING16(L"country code");
  position.address.postal_code   = STRING16(L"postal code");

  Position updated_position;
  updated_position.latitude              = 11.0;
  updated_position.longitude             = 12.0;
  updated_position.altitude              = 13;
  updated_position.horizontal_accuracy   = 14;
  updated_position.vertical_accuracy     = 15;
  updated_position.timestamp             = 16;
  updated_position.error                 = STRING16(L"updated error");
  updated_position.address.street_number = STRING16(L"updated street number");
  updated_position.address.street        = STRING16(L"updated street");
  updated_position.address.premises      = STRING16(L"updated premises");
  updated_position.address.city          = STRING16(L"updated city");
  updated_position.address.county        = STRING16(L"updated county");
  updated_position.address.region        = STRING16(L"updated region");
  updated_position.address.country       = STRING16(L"updated country");
  updated_position.address.country_code  = STRING16(L"updated country code");
  updated_position.address.postal_code   = STRING16(L"updated postal code");

  Position retrieved_position;

  // Store a position.
  if (!db->StorePosition(kTestPositionString, position)) {
    *error += STRING16(L"TestGeolocationDB(): Failed to store position. ");
    return false;
  }

  // Retrieve the position.
  if (!db->RetrievePosition(kTestPositionString, &retrieved_position)) {
    *error += STRING16(L"TestGeolocationDB(): Failed to retrieve position. ");
    return false;
  }
  if (!ArePositionsEqual(retrieved_position, position)) {
    *error += STRING16(L"TestGeolocationDB(): Retrieved position does not "
                       L"match original. ");
    return false;
  }

  // Replace the position.
  if (!db->StorePosition(kTestPositionString, updated_position)) {
    *error += STRING16(L"TestGeolocationDB(): Failed to update position. ");
    return false;
  }

  // Retrieve the new position.
  if (!db->RetrievePosition(kTestPositionString, &retrieved_position)) {
    *error += STRING16(L"TestGeolocationDB(): Failed to retrieve updated "
                       L"position. ");
    return false;
  }
  if (!ArePositionsEqual(retrieved_position, updated_position)) {
    *error += STRING16(L"TestPositionTable(): Retrieved updated position does "
                       L"not match original. ");
    return false;
  }

  return true;
}

// Local functions
static bool AreAddressesEqual(const Address &left, const Address &right) {
  return left.street_number == right.street_number &&
         left.street        == right.street &&
         left.premises      == right.premises &&
         left.city          == right.city &&
         left.county        == right.county &&
         left.region        == right.region &&
         left.country       == right.country &&
         left.country_code  == right.country_code &&
         left.postal_code   == right.postal_code;
}

static bool ArePositionsEqual(const Position &left, const Position &right) {
  return left.latitude            == right.latitude &&
         left.longitude           == right.longitude &&
         left.altitude            == right.altitude &&
         left.horizontal_accuracy == right.horizontal_accuracy &&
         left.vertical_accuracy   == right.vertical_accuracy &&
         left.timestamp           == right.timestamp &&
         left.error               == right.error &&
         AreAddressesEqual(left.address, right.address);
}

#endif  // USING_CCTESTS
