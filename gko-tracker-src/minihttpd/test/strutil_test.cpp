#include <pthread.h>
#include "libminihttpd/strutil.h"
#include "gtest/gtest.h"

namespace argus {
namespace common {

TEST(StrUtilTest, Ops) {
  string str("  Hello World ");
  StringTrimLeft(&str);
  EXPECT_EQ("Hello World ", str);
  StringTrimRight(&str);
  EXPECT_EQ("Hello World", str);
  StringToLower(&str);
  EXPECT_EQ("hello world", str);

  string str2("  Hello World ");
  StringTrim(&str2);
  EXPECT_EQ("Hello World", str2);
}

}
}

