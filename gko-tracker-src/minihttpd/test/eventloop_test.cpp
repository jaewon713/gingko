#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include "libminihttpd/eventloop.h"
#include "gtest/gtest.h"

::argus::common::EventLoop  loop;

void oneTimeSchedulerFunc(int fd, short event, void *args) {
  LOG(WARNING) << "I run only once";
}

void periodSchedulerFunc(int fd, short event, void *args) {
  LOG(ERROR) << "a period func~~~";
}

void exitFunc(int fd, short event, void *args) {
  EXPECT_EQ(0, loop.quit());
}

TEST(EventLoopTest, EventLoopTest) {
  loop.runEvery(2.5, periodSchedulerFunc);
  loop.runAfter(3, oneTimeSchedulerFunc);
  loop.runAfter(6, exitFunc);

  LOG(INFO) << "EventLoop::loop() starts";
  int ret = loop.loop();
  LOG(INFO) << "EventLoop::loop() returns " << ret;
}

