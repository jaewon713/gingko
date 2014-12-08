#ifndef ARGUS_COMMON_PROCESSINSPECTOR_H_
#define ARGUS_COMMON_PROCESSINSPECTOR_H_

#include "common.h"

namespace argus {
namespace common {

class MiniHttpd;

class ProcessInspector {
public:
  void registerCallBacks(MiniHttpd* httpd);

private:
  static string baseInfo(const string&);
  static string pid(const string&);
  static string procStatus(const string&);
  static string openedFiles(const string&);
  static string threads(const string&);
  static string startTime(const string&);
  static string account(const string&);
  static string binaryPath(const string&);
};

} // namespace common
} // namespace argus

#endif  // ARGUS_COMMON_PROCESSINSPECTOR_H_

