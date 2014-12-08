/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   ErrorCategory.cpp
 *
 * @author liuming03
 * @date   2014-1-15
 * @brief 
 */

#include "bbts/ErrorCategory.h"

using std::string;
using boost::system::error_category;

namespace bbts {

const char *ErrorCategory::name_ = "bbts error";

string ErrorCategory::message(int ev) const {
  const static char* msgs[] = {
      "no error",
      "memery alloc failed",
      "message read failed",
      "ptotobuf parse failed",
      "args not correct",
      "torrent file not correct",
      "infohash not correct",
      "dup task error",
      "generate taskid failed",
      "add torrent failed",
      "task not found",
      "tracker failed",
      "read cred failed",
      "check cred failed",
      "db error",
      "have no metadata",
      "parse metadata failed",
  };
  if (ev < 0 || ev >= errors::MAX_ERROR_NUM) {
    return string("invalid error");
  }
  return msgs[ev];
}

const error_category& get_error_category() {
  const static ErrorCategory category;
  return category;
}

} // namespace bbts
