#include "minihttpd/current_thread.h"
#include <sys/types.h>
#include <sys/syscall.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

namespace argus {
namespace CurrentThread {

__thread int t_cachedTid = 0;
__thread char t_tidString[32];
__thread const char* t_threadName = "unknown";

namespace internal {

pid_t gettid() {
  return static_cast<pid_t>(::syscall(SYS_gettid));
}

} // namespace internal

void cacheTid() {
  if (t_cachedTid == 0) {
    t_cachedTid = internal::gettid();
    snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
  }
}

bool isMainThread() {
  return tid() == ::getpid();
}

} // namespace CurrentThread
} // namespace argus

