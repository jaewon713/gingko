#ifndef ARGUS_COMMON_SPINLOCK_H_
#define ARGUS_COMMON_SPINLOCK_H_

#include "current_thread.h"
#include "common.h"
#include <assert.h>
#include <pthread.h>

namespace argus {
namespace common {

class SpinLock {
public:
  SpinLock()
    : holder_(0)
  {
    int ret = pthread_spin_init(&spinlock_, 0) ;
    assert(ret == 0);
  }

  ~SpinLock() {
    assert(holder_ == 0);
    int ret = pthread_spin_destroy(&spinlock_);
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
    pthread_spin_lock(&spinlock_);
    holder_ = CurrentThread::tid();
  }

  void unlock() {
    holder_ = 0;
    pthread_spin_unlock(&spinlock_);
  }

  pthread_spinlock_t* getPthreadSpinLock() {
    return &spinlock_;
  }

private:
  ARGUS_DISALLOW_EVIL_CONSTRUCTORS(SpinLock);
  pthread_spinlock_t spinlock_;
  pid_t holder_;
};

class SpinLockGuard {
public:
  explicit SpinLockGuard(SpinLock &spinlock)
    : spinlock_(spinlock)
  {
    spinlock_.lock();
  }

  ~SpinLockGuard() {
    spinlock_.unlock();
  }

private:
  ARGUS_DISALLOW_EVIL_CONSTRUCTORS(SpinLockGuard);
  SpinLock &spinlock_;
};

} // namespace common
} // namespace argus

// Prevent misuse like:
// SpinLockGuard(spinlock_);
// A tempory object doesn't hold the lock for long!
#define SpinLockGuard(x) error "Missing guard object name"

#endif  // ARGUS_COMMON_SPINLOCK_H_
