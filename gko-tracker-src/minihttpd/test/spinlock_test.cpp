#include <pthread.h>
#include "libminihttpd/spinlock.h"
#include "gtest/gtest.h"

using ::argus::common::SpinLock;
using ::argus::common::SpinLockGuard;

int64_t g_intVal = 0;
SpinLock lock;

void *add_thread(void *args) {
  for (int i = 0; i < 10000; i++) {
    SpinLockGuard lock_guard(lock);
    g_intVal++;
  }
  return (void *) 0;
}

TEST(SpinLockTest, SpinLockTest) {
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

