#include "libminihttpd/eventloop.h"
#include "libminihttpd/minihttpd.h"
#include "libminihttpd/scoped_ptr.h"
#include <pthread.h>
#include <unistd.h>

using ::argus::common::EventLoop;
using ::argus::common::scoped_ptr;

scoped_ptr<EventLoop> g_loop;

void oneTimeSchedulerFunc(int fd, short event, void *args) {
  char *data = static_cast<char *>(args);
  if (NULL == data) {
    data="pass no data";
  }
  LOG(WARNING) << "only once! " << data;
}

void periodSchedulerFunc(int fd, short event, void *args) {
  char *data = static_cast<char *>(args);
  if (NULL == data) {
    data="pase null data";
  }
  LOG(ERROR) << "a period func! " << data;
}

void* thread1VisitEvLoop(void *args) {
  char *data = "registered in thread1, runAfter 10s";
  g_loop->runAfter(10, oneTimeSchedulerFunc, data);
  return (void*)0;
}

void* thread2VisitEvLoop(void *args) {
  char *data = "registered in thread2, runAfter 13s";
  g_loop->runAfter(13, oneTimeSchedulerFunc, data);
  return (void*)0;
}

void* thread3VisitEvLoop(void *args) {
  char *data = "registered in thread3, runEvery 7s";
  g_loop->runEvery(7, periodSchedulerFunc, data);
  return (void*)0;
}

int main(int argc, char* argv[]) {
  g_loop.reset(CHECK_NOTNULL(new(std::nothrow) EventLoop));

  pthread_t tid[3];
  int ret = pthread_create(&tid[0], NULL, thread1VisitEvLoop, NULL);
  assert(0 == ret);
  ret = pthread_create(&tid[1], NULL, thread2VisitEvLoop, NULL);
  assert(0 == ret);
  ret = pthread_create(&tid[2], NULL, thread3VisitEvLoop, NULL);
  assert(0 == ret);

  char *des =  "registered in mainThread, runEvery 2.5s";
  g_loop->runEvery(2.5, periodSchedulerFunc, des);
  g_loop->runAfter(3, oneTimeSchedulerFunc);

  LOG(INFO) << "EventLoop::loop() starts";
  ret = g_loop->loop();
  LOG(INFO) << "EventLoop::loop() returns " << ret;
}

