#ifndef ARGUS_COMMON_PROCESSINFO_H_
#define ARGUS_COMMON_PROCESSINFO_H_

#include <vector>
#include <algorithm>
#include <dirent.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <sys/syscall.h>
#include "types.h"

namespace argus {
namespace common {
namespace ProcessInfo {

pid_t gettid(void);

pid_t pid();
string pidString();

uid_t uid();
string uidString();
string username();

uid_t euid();
string euidString();

string startTime();
string hostname();

/// read /proc/self/status
string procStatus();

int openedFiles();
int maxOpenFiles();

int numThreads();
std::vector<pid_t> threads();

string binaryPath();
string binaryDirectory();
string binaryName();

} // namespace ProcessInfo
} // namespace common
} // namespace argus

#endif  // ARGUS_COMMON_PROCESSINFO_H_

