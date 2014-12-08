#ifndef ARGUS_COMMON_MUTEX_H_
#define ARGUS_COMMON_MUTEX_H_

#include "current_thread.h"
#include "common.h"
#include <assert.h>
#include <pthread.h>

namespace argus {
namespace common {

class MutexLock {
public:
  MutexLock()
    : holder_(0)
  {
    int ret = pthread_mutex_init(&mutex_, NULL);
    assert(ret == 0);
  }

  ~MutexLock() {
    assert(holder_ == 0);
    int ret = pthread_mutex_destroy(&mutex_);
    assert(ret == 0);
  }

  bool isLockedByThisThread() const {
    return holder_ == CurrentThread::tid();
  }

  void assertLocked() const {
    assert(isLockedByThisThread());
  }

  // internal usage
  void lock() {
    pthread_mutex_lock(&mutex_);
    holder_ =  CurrentThread::tid();
  }

  void unlock() {
    holder_ = 0;
    pthread_mutex_unlock(&mutex_);
  }

  // non-const
  pthread_mutex_t* getPthreadMutex() {
    return &mutex_;
  }

private:
  ARGUS_DISALLOW_EVIL_CONSTRUCTORS(MutexLock);
  pthread_mutex_t mutex_;
  pid_t holder_;
};

class MutexLockGuard {
public:
  explicit MutexLockGuard(MutexLock& mutex)
    : mutex_(mutex)
  {
    mutex_.lock();
  }

  ~MutexLockGuard() {
    mutex_.unlock();
  }

private:
  ARGUS_DISALLOW_EVIL_CONSTRUCTORS(MutexLockGuard);
  MutexLock& mutex_;
};

} // namespace common
} // namespace argus

// Prevent misuse like:
// MutexLockGuard(mutex_);
// A tempory object doesn't hold the lock for long!
#define MutexLockGuard(x) error "Missing guard object name"

#endif  // ARGUS_COMMON_MUTEX_H_

