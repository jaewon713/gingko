#include <pthread.h>
#include "libminihttpd/process_info.h"
#include "gtest/gtest.h"
#include "glog/logging.h"

namespace argus {
namespace common {

TEST(ProcessInfo, Ops) {
  EXPECT_EQ(ProcessInfo::gettid(), ProcessInfo::pid());
  LOG(INFO) << "pid:" << ProcessInfo::pidString();

  LOG(INFO) << "uid:" << ProcessInfo::uid();
  LOG(INFO) << "uidString:" << ProcessInfo::uidString();
  LOG(INFO) << "username:" << ProcessInfo::username();

  LOG(INFO) << "euid:" << ProcessInfo::euid();
  LOG(INFO) << "euidString:" << ProcessInfo::euidString();


  LOG(INFO) << "startTime:" << ProcessInfo::startTime();
  LOG(INFO) << "hostname:" << ProcessInfo::hostname();

  LOG(INFO) << "procStatus:" << ProcessInfo::procStatus();
  LOG(INFO) << "openedFiles:" << ProcessInfo::openedFiles();
  LOG(INFO) << "maxOpenFiles:" << ProcessInfo::maxOpenFiles();
  LOG(INFO) << "numThreads:" << ProcessInfo::numThreads();


  LOG(INFO) << "binaryPath:" << ProcessInfo::binaryPath();
  LOG(INFO) << "binaryDirectory:" << ProcessInfo::binaryDirectory();
  LOG(INFO) << "binaryName:" << ProcessInfo::binaryName();
  EXPECT_EQ("process_info_test", ProcessInfo::binaryName());
}

}
}

