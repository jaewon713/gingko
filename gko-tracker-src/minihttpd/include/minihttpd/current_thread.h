#ifndef ARGUS_COMMON_CURRENTTHREAD_H_
#define ARGUS_COMMON_CURRENTTHREAD_H_

namespace argus {
namespace CurrentThread {

// internal
extern __thread int t_cachedTid;
extern __thread char t_tidString[32];
extern __thread const char *t_threadName;
void cacheTid();

inline int tid() {
  if (t_cachedTid == 0) {
    cacheTid();
  }
  return t_cachedTid;
}

// for logging
inline const char* tidString() {
  return t_tidString;
}

inline const char* name() {
  return t_threadName;
}

bool isMainThread();

} // namespace CurrentThread
} // namespace argus

#endif  // ARGUS_COMMON_CURRENTTHREAD_H_

