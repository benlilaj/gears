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

#ifdef OFFICIAL_BUILD
  // The notification API has not been finalized for official builds.
#else
#if USING_CCTESTS
#include <utility>

#include "gears/notifier/notification_manager_test.h"

#include "gears/base/common/basictypes.h"
#include "gears/base/common/common.h"
#include "gears/base/common/stopwatch.h"
#include "gears/base/common/string16.h"
#include "gears/notifier/notification.h"
#include "gears/notifier/notification_manager.h"
#include "gears/notifier/unit_test.h"
#include "gears/notifier/user_activity.h"

// This method is defined in the _test file so that notification_manager.cc
// has no dependencies on test files (even when USING_CCTEST is defined).
BalloonCollectionMock *NotificationManager::UseBalloonCollectionMock() {
  BalloonCollectionMock *mock = new BalloonCollectionMock;
  balloon_collection_.reset(mock);
  return mock;
}

#undef TEST_ASSERT
#define TEST_ASSERT(test, message)                                      \
  do {                                                                  \
    if (!(test)) {                                                      \
      std::string16 final_message(                                      \
          STRING16(L"Notifier Test Failed"));       \
      final_message.append(message);                                    \
      LogTestError(final_message);                                      \
    }                                                                   \
  } while (0)

BalloonCollectionMock::BalloonCollectionMock()
    : has_space_(false),
      show_call_count_(0),
      update_call_count_(0),
      delete_call_count_(0) {
}

BalloonCollectionMock::~BalloonCollectionMock() {
  TEST_ASSERT(show_call_count_ == 0,
              STRING16(L"Not enough show calls done."));
  TEST_ASSERT(update_call_count_ == 0,
              STRING16(L"Not enough update calls done."));
  TEST_ASSERT(delete_call_count_ == 0,
              STRING16(L"Not enough delete calls done."));
}

void BalloonCollectionMock::Show(const GearsNotification &notification) {
  NotificationId id(notification.service(), notification.id());
  TEST_ASSERT(displayed_.find(id) == displayed_.end(),
              STRING16(L"Already showing notification."));
  TEST_ASSERT(show_call_count_ > 0,
              STRING16(L"Unexpected show call."));
  show_call_count_--;

  displayed_[id] = 1;
}

bool BalloonCollectionMock::Update(const GearsNotification &notification) {
  NotificationId id(notification.service(), notification.id());
  if (displayed_.find(id) == displayed_.end()) {
    return false;
  }
  update_call_count_--;
  displayed_[id] = displayed_[id] + 1;
  return true;
}

bool BalloonCollectionMock::Delete(const std::string16 &service,
                                   const std::string16 &bare_id) {
  NotificationId id(service, bare_id);
  if (displayed_.find(id) == displayed_.end()) {
    return false;
  }
  delete_call_count_--;
  displayed_.erase(id);
  return true;
}

bool BalloonCollectionMock::has_space() {
  return has_space_;
}

void BalloonCollectionMock::set_has_space(bool has_space) {
  has_space_ = has_space;
}

void BalloonCollectionMock::set_show_call_count(int show_call_count) {
  TEST_ASSERT(show_call_count_ == 0,
              STRING16(L"Not enough show calls done."));
  show_call_count_ = show_call_count;
}

void BalloonCollectionMock::set_update_call_count(int update_call_count) {
  TEST_ASSERT(update_call_count_ == 0,
              STRING16(L"Not enough update calls done."));
  update_call_count_ = update_call_count;
}

void BalloonCollectionMock::set_delete_call_count(int delete_call_count) {
  TEST_ASSERT(delete_call_count_ == 0,
              STRING16(L"Not enough delete calls done."));
  delete_call_count_ = delete_call_count;
}

class UserActivityMock : public UserActivityInterface {
 public:
  UserActivityMock() : user_mode_(USER_NORMAL_MODE) {
  }

  virtual UserMode CheckUserActivity() {
    return user_mode_;
  }

  void set_user_mode(UserMode user_mode) {
    user_mode_ = user_mode;
  }

 private:
  UserMode user_mode_;
  DISALLOW_EVIL_CONSTRUCTORS(UserActivityMock);
};

void TestNotificationManager() {
  UserActivityMock activity;
  NotificationManager manager(&activity);
  BalloonCollectionMock *balloon_collection =
      manager.UseBalloonCollectionMock();

  // Start with no room in the balloon collection.
  balloon_collection->set_has_space(false);

  // Add a notification when there is no space.
  GearsNotification notification1;
  notification1.set_service(STRING16(L"http://gears.google.com/MyService"));
  notification1.set_id(STRING16(L"1"));
  manager.Add(notification1);

  // Make space available and expect the balloon to be shown.
  balloon_collection->set_has_space(true);
  balloon_collection->set_show_call_count(1);
  manager.OnBalloonSpaceChanged();

  // Update the notification with no space available.
  balloon_collection->set_has_space(false);
  balloon_collection->set_update_call_count(1);
  manager.Add(notification1);

  // Add a notification while the user is away.
  balloon_collection->set_has_space(true);
  activity.set_user_mode(USER_AWAY_MODE);

  GearsNotification notification2;
  notification2.set_service(STRING16(L"http://gears.google.com/MyService"));
  notification2.set_id(STRING16(L"2"));
  manager.Add(notification2);

  // Go through all of the modes and ensure that the
  // notification doesn't get displayed.
  activity.set_user_mode(USER_IDLE_MODE);
  manager.OnUserActivityChange();

  activity.set_user_mode(USER_INTERRUPTED_MODE);
  manager.OnUserActivityChange();

  activity.set_user_mode(USER_MODE_UNKNOWN);
  manager.OnUserActivityChange();

  activity.set_user_mode(USER_PRESENTATION_MODE);
  manager.OnUserActivityChange();

  // Transitioning to normal mode should cause the notification to appear.
  activity.set_user_mode(USER_NORMAL_MODE);
  balloon_collection->set_show_call_count(1);
  manager.OnUserActivityChange();
}

void RunMessageLoop(int max_time_ms) {
  // TODO(levin): Set-up timer for max_timer_ms that calls
  // a function which quits the message loop.

#ifdef WIN32
  while (true) {
    MSG message;
    // Returns 0 if WM_QUIT, else non-zero (but -1 if error)
    BOOL ret = GetMessage(&message, NULL, 0, 0);
    if (!ret) {
      break;
    }
    if (ret != -1) {
      TranslateMessage(&message);
      DispatchMessage(&message);
    }
  }
  // To quit: PostQuitMessage(0);
#elif defined(OS_MACOSX)
  // Loop, waiting for events to be processed.
  // Likely: CFRunLoopRun();
  // To quit: CFRunLoopStop(CFRunLoopGetCurrent());
#elif defined(LINUX)
  // TODO(levin): fill this in depending on what is done for the timers.
  // Likely: gtk_main();
  // To quit: gtk_main_quit();
#endif
}

void TestNotificationManagerDelay() {
  UserActivityMock activity;
  NotificationManager manager(&activity);
  BalloonCollectionMock *balloon_collection =
      manager.UseBalloonCollectionMock();

  // Start with room in the balloon collection.
  balloon_collection->set_has_space(true);

  // Add a notification when there is no space.
  GearsNotification notification1;
  notification1.set_service(STRING16(L"http://gears.google.com/MyService"));
  notification1.set_id(STRING16(L"1"));
  notification1.set_display_at_time_ms(GetCurrentTimeMillis() + 500);
  manager.Add(notification1);

  // TODO(levin): enable the following when timer is working.

  // balloon_collection->set_show_call_count(1);
  // // Wait for the 1/2 sec to expire
  // RunMessageLoop(500);
  // // Timers aren't exact, so if the balloon hasn't displayed,
  // // give it a 1/2 second more to be displayed.
  // if (balloon_collection->show_call_count()) {
  //   RunMessageLoop(500);
  // }

  // TODO(levin): Test 2: do an update in which the display at time changes

  // TODO(levin): Test 3: Try snooze with an old display at time
  // (and then update with a new display at time in the past).

  // See NotificationManager::Add for several other test cases around updating
  // notifications with a new display at time.
}

#endif  // USING_CCTESTS
#endif  // OFFICIAL_BUILD
