#ifndef ARGUS_COMMON_MONITOR_HELPER_H_
#define ARGUS_COMMON_MONITOR_HELPER_H_

#include "common.h"
#include "spinlock.h"
#include <map>

namespace argus {
namespace common {

// thread-safe container
//
class DataContainer {
public:
  DataContainer() {}
  ~DataContainer() {}

  void formatToString(string *output);

  // set key-val pair to container
  void setKeyVal(string key, string val);

  void setKeyVal(string key, int64_t val);
  void increment(string key, int64_t val = 1);

  void setKeyVal(string key, double val);

private:
  ARGUS_DISALLOW_EVIL_CONSTRUCTORS(DataContainer);
  std::map<string, string>  strMap_;
  std::map<string, int64_t> intMap_;
  std::map<string, double>  floatMap_;
  SpinLock lock_;
};

} // namespace common
} // namespace argus

#endif  // ARGUS_COMMON_MONITOR_HELPER_H_

