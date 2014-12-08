#include <pthread.h>
#include "libminihttpd/mutex.h"
#include "gtest/gtest.h"

using ::argus::common::MutexLock;
using ::argus::common::MutexLockGuard;

int64_t g_intVal = 0;
MutexLock lock;

void *add_thread(void *args) {
  for (int i = 0; i < 10000; i++) {
    MutexLockGuard lock_guard(lock);
    g_intVal++;
    assert(lock.isLockedByThisThread());
  }
  return (void *) 0;
}

TEST(MutexTest, MutexTest) {
  pthread_t threads[40];
  for (int i = 0; i < 40; i++) {
    int ret = pthread_create(&threads[i], NULL, add_thread, NULL);
    assert(0 == ret);
  }

  for (int i = 0; i < 40; i++) {
    pthread_join(threads[i], NULL);
  }

  EXPECT_EQ(400000, g_intVal);
}

