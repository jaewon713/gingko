#include <stdio.h>
#include "minihttpd/monitor_helper.h"

namespace argus {
namespace common {

void DataContainer::setKeyVal(string key, string val) {
  SpinLockGuard lock(lock_);
  strMap_[key] = val;
}

void DataContainer::setKeyVal(string key, int64_t val) {
  SpinLockGuard lock(lock_);
  intMap_[key] = val;
}

void DataContainer::increment(string key, int64_t val) {
  std::map<string, int64_t>::iterator it;

  SpinLockGuard lock(lock_);
  it = intMap_.find(key);
  if (it == intMap_.end()) {
    intMap_[key] = val;
  }
  else {
    intMap_[key] += val;
  }
}

void DataContainer::setKeyVal(string key, double val) {
  SpinLockGuard lock(lock_);
  floatMap_[key] = val;
}

void DataContainer::formatToString(string *output) {
  std::map<string, string>  strMap;
  std::map<string, int64_t> intMap;
  std::map<string, double>  floatMap;

  {
    SpinLockGuard lock(lock_);
    strMap = strMap_;
    intMap = intMap_;
    floatMap = floatMap_;
  }

  output->clear();

  std::map<string, string>::iterator strIt;
  for (strIt = strMap.begin(); strIt != strMap.end(); ++strIt) {
    output->append(strIt->first);
    output->append(":");
    output->append(strIt->second);
    output->append("\n");
  }

  std::map<string, int64_t>::iterator intIt;
  for (intIt = intMap.begin(); intIt != intMap.end(); ++intIt) {
    char buf[64];
    snprintf(buf, sizeof buf, "%ld\n", intIt->second);
    output->append(intIt->first);
    output->append(":");
    output->append(buf);
  }

  std::map<string, double>::iterator floatIt;
  for (floatIt = floatMap.begin(); floatIt != floatMap.end(); ++floatIt) {
    char buf[64];
    snprintf(buf, sizeof buf, "%f\n", floatIt->second);
    output->append(floatIt->first);
    output->append(":");
    output->append(buf);
  }

}





} // namespace common
} // namespace argus


