#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "libminihttpd/current_thread.h"
#include "gtest/gtest.h"

TEST(current_threadTest, current_threadTest) 
{
  int tid = ::argus::CurrentThread::tid();
  printf("tid = %d\n", tid);
  EXPECT_EQ(true, ::argus::CurrentThread::isMainThread());
}

