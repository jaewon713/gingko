#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include "libminihttpd/monitor_helper.h"
#include "gtest/gtest.h"

TEST(MonitorHelperTest, DataContainerTest) {
  ::argus::common::DataContainer dc;
  dc.setKeyVal("key1", "val1");
  dc.setKeyVal("key2", static_cast<int64_t>(2));
  dc.increment("key2", 1);
  dc.setKeyVal("key3", 2.5);

  ::std::string formattedStr;
  dc.formatToString(&formattedStr);

  LOG(INFO) << "dc.formatToString() " << formattedStr;
}

