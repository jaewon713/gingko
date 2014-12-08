#include "libminihttpd/eventloop.h"
#include "libminihttpd/minihttpd.h"

void oneTimeSchedulerFunc(int fd, short event, void *args) {
  LOG(WARNING) << "I run only once";
}

void periodSchedulerFunc(int fd, short event, void *args) {
  LOG(ERROR) << "a period func~~~";
}

int main(int argc, char* argv[]) {
  ::argus::common::EventLoop  loop;

  uint64_t timerId1 = loop.runEvery(2.5, periodSchedulerFunc);
  uint64_t timerId2 = loop.runAfter(6.4, oneTimeSchedulerFunc);

  LOG(INFO) << "EventLoop::loop() starts";
  // loop.cancel(timerId1);
  int ret = loop.loop();
  LOG(INFO) << "EventLoop::loop() returns " << ret;
}

