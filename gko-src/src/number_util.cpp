/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   number_util.cpp
 *
 * @author liuming03
 * @date   2013-11-18
 * @brief 
 */

#include "bbts/number_util.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

namespace bbts {

void BytesToReadable(int64_t bytes, std::string *bytes_string) {
  assert(bytes >= 0);
  assert(bytes_string);
  int64_t div = 1LL;
  const char *unit = "B";
  if (bytes >= 1024LL * 1024 * 1024 * 1024) {
    div = 1024LL * 1024 * 1024 * 1024;
    unit = "TB";
  } else if (bytes >= 1024LL * 1024 * 1024) {
    div = 1024LL * 1024 * 1024;
    unit = "GB";
  } else if (bytes >= 1024LL * 1024) {
    div = 1024LL * 1024;
    unit = "MB";
  } else if (bytes >= 1024LL) {
    div = 1024LL;
    unit = "KB";
  }

  double result = static_cast<double>(bytes) / div;
  char buf[20] = { 0 };
  snprintf(buf, sizeof(buf), "%.2f%s", result, unit);
  *bytes_string = buf;
}

int64_t ReadableToBytes(const std::string &bytes_string, int64_t unit) {
  int64_t bytes;
  if (bytes_string.empty()) {
    bytes = 0LL;
  }
  switch (*bytes_string.rbegin()) {
    case 'M':
      // fall through
    case 'm': {
      unit = 1024LL * 1024;
      break;
    }

    case 'K':
      // fall through
    case 'k': {
      unit = 1024LL;
      break;
    }

    case 'G':
      // fall through
    case 'g': {
      unit = 1024LL * 1024 * 1024;
      break;
    }

    case 'T':
      // fall through
    case 't': {
      unit = 1024LL * 1024 * 1024 * 1024;
      break;
    }

    case 'B':
      // fall through
    case 'b': {
      unit = 1;
      break;
    }

    default: {
      break;
    }
  }
  // atof自动忽略数字后面的字母后缀
  bytes = static_cast<int64_t>(atof(bytes_string.c_str()) * unit);
  return bytes;
}

} // namespace bbts
