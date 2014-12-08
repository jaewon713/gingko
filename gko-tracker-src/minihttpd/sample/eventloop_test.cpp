#include "libminihttpd/eventloop.h"
#include "libminihttpd/minihttpd.h"
#include <string>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

using ::argus::common::MiniHttpd;
using ::argus::common::EventLoop;

static std::string callbackOne(const std::string &query) {
  return (query + " hello minihttpd~~~ \n");
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
  EventLoop loop;
  MiniHttpd httpd(&loop, 8889);

  httpd.enableLogging(false);
  httpd.enableIpFilter(true);

  bool succ = httpd.setCallBack("hellocb", callbackOne);
  assert(succ);

  int ret = loop.loop();
  LOG(INFO) << "EventLoop::loop() returns " << ret;
}
