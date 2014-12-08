#include "minihttpd/process_inspector.h"
#include "minihttpd/process_info.h"
#include "minihttpd/minihttpd.h"
#include <stdio.h>

namespace argus {
namespace common {

void ProcessInspector::registerCallBacks(MiniHttpd* httpd) {
  string retmsg;
  if (!httpd->setCallBack(MiniHttpd::baseInfoPath, ProcessInspector::baseInfo, &retmsg)) {
    LOG(ERROR) << "failed registerCallBacks" << retmsg;
  }
  // httpd->setCallBack("start_time", ProcessInspector::startTime);
  // httpd->setCallBack("account", ProcessInspector::account);
  // httpd->setCallBack("binary_path", ProcessInspector::binaryPath);
  // httpd->setCallBack("proc/pid", ProcessInspector::pid);
  // httpd->setCallBack("proc/status", ProcessInspector::procStatus);
  // httpd->setCallBack("proc/opened_files", ProcessInspector::openedFiles);
  // httpd->setCallBack("proc/threads", ProcessInspector::threads);
}

string ProcessInspector::baseInfo(const string&) {
  string content;
  char buf[32];

  // binaryPath
  content += "binary:" + ProcessInspector::binaryPath(content);

  // startTime
  content += "startTime:" + ProcessInspector::startTime(content);

  // startAccount
  content += ProcessInspector::account(content);

  // pid
  snprintf(buf, sizeof buf, "pid:%d\n", ProcessInfo::pid());
  content += buf;

  // openedFiles
  snprintf(buf, sizeof buf, "openedFiles:%d\n", ProcessInfo::openedFiles());
  content += buf;

  // proc/status
  content += ProcessInfo::procStatus();

  return content;
}

string ProcessInspector::pid(const string&) {
  char buf[32];
  snprintf(buf, sizeof buf, "%d\n", ProcessInfo::pid());
  return buf;
}

string ProcessInspector::procStatus(const string&) {
  return ProcessInfo::procStatus();
}

string ProcessInspector::openedFiles(const string&) {
  char buf[32];
  snprintf(buf, sizeof buf, "%d\n", ProcessInfo::openedFiles());
  return buf;
}

string ProcessInspector::threads(const string&) {
  std::vector<pid_t> threads = ProcessInfo::threads();
  string result;
  for (size_t i = 0; i < threads.size(); ++i) {
    char buf[32];
    snprintf(buf, sizeof buf, "%d\n", threads[i]);
    result += buf;
  }
  return result;
}

string ProcessInspector::startTime(const string&) {
  return ProcessInfo::startTime() + "\n";
}

string ProcessInspector::account(const string&) {
  string result = "uid:";
  result += ProcessInfo::uidString();
  result += "\n";

  result += "euid:";
  result += ProcessInfo::euidString();
  result += "\n";

  result += "username:";
  result += ProcessInfo::username();
  result += "\n";
  return result;
}

string ProcessInspector::binaryPath(const string&) {
  string path = ProcessInfo::binaryPath();
  return path + "\n";
}


} // namespace common
} // namespace argus

