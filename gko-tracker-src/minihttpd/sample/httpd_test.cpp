#include "libminihttpd/eventloop.h"
#include "libminihttpd/minihttpd.h"
#include "libminihttpd/monitor_helper.h"
#include <string>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

using ::argus::common::DataContainer;
using ::argus::common::scoped_ptr;
using ::argus::common::MiniHttpd;
using ::argus::common::EventLoop;

scoped_ptr<MiniHttpd> g_Httpserver;
scoped_ptr<DataContainer> g_MonitorData;

std::string path("example");

// get formattd current time
std::string getTimeStr() {
  // 30 bytes is enough to hold "[year-month-day : hour-minute-second]"
  char time[30] = { 0 };
  const char *format = "[%Y-%m-%d %H:%M:%S]";

  struct timeval tv;
  struct tm ltime;
  time_t curtime;
  gettimeofday(&tv, NULL);
  curtime = tv.tv_sec;

  // Format time
  strftime(time, 30, format, localtime_r(&curtime, &ltime));
  return time;
}

std::string callbackOne(const std::string &query) {
  return (query + " hello argus\n");
}

void *upInfoThread(void *args) {
  std::string hint;

  while (true) {
    // balabala... Caculate the info need to show
    g_MonitorData->setKeyVal("current", getTimeStr());

    g_MonitorData->setKeyVal("timestamp", time(NULL));
    g_MonitorData->increment("timestamp");

    g_MonitorData->setKeyVal("doubleKey", 1.2);

    std::string formattdStr;
    g_MonitorData->formatToString(&formattdStr);

    // path format need to be ([a-z]+)
    g_Httpserver->setResponse(path, formattdStr);

    bool ok = g_Httpserver->setResponse("NOt&@Valid", formattdStr, &hint);
    if (!ok) {
      LOG(ERROR) << "setResponse failed: " << hint;
    }

    sleep(1);
  }
  return (void *) 0;
}

int main(int argc, char *argv[]) {
  // Usage : MiniHttpd has two ways to initialize
  // (1) simple way, just need one line of code like below
  //     But please remeber that in this way, 
  //     program will be aborted if @port bind failed. Be care!!!!
  //
  // ::argus::common::MiniHttpd httpserver(&loop, port);
  //
  // (2) Second way is divided into two steps.
  //     firstly construct a MiniHttpd instance named httpserver;
  //     Secondly call httpserver.bind(port);
  //     bind() will fail and return false when @port already in use
  //
  // ::argus::common::MiniHttpd httpserver(&loop);
  // bool success = httpserver.bind(port);
  // if (!success) {
  //     LOG(ERROR) << "port bind failed";
  //     return EXIT_FAIURE;
  // }
  //

  uint16_t port = 8888;
  EventLoop loop;
  g_MonitorData.reset(CHECK_NOTNULL(new(std::nothrow) DataContainer));
  g_Httpserver.reset(CHECK_NOTNULL(new(std::nothrow) MiniHttpd(&loop, port)));

  pthread_t tid;
  int ret = pthread_create(&tid, NULL, upInfoThread, NULL);
  assert(0 == ret);

  g_Httpserver->setCallBack("hellocb", callbackOne);
  // std::string retmsg;
  // bool ok = g_Httpserver->setCallBack("hellocb", callbackOne, &retmsg);
  // if (!ok) {
  //   LOG(ERROR) << "setCallBack failed: " << retmsg;
  // }

  g_MonitorData->setKeyVal("versionNeverChanged", "0.0.1.0");
  std::string test("a\0bc", 4);
  g_Httpserver->setResponse("test", test);
  g_Httpserver->setResponse("Test.-_9", "path should match ([A-Za-z0-9.-_]+)");

  ret = loop.loop();
  LOG(INFO) << "EventLoop::loop() returns " << ret;
}
