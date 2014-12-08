/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   TrackerErrorCategory.cpp
 *
 * @author liuming03
 * @date   2014-2-26
 * @brief 
 */

#include "bbts/tracker/TrackerErrorCategory.h"

using std::string;
using boost::system::error_category;

namespace bbts {
namespace tracker {

const char *TrackerErrorCategory::name_ = "bbts error";

string TrackerErrorCategory::message(int ev) const {
  const static char* msgs[] = {
      "no error",
      "missing arguments",
      "invalid arguments",
      "infohash not found",
      "invalid operate type",
  };
  if (ev < 0 || ev >= errors::MAX_ERROR_NUM) {
    return string("invalid error");
  }
  return msgs[ev];
}

const error_category& get_error_category() {
  const static TrackerErrorCategory category;
  return category;
}

}  // namespace tracker
}  // namespace bbts
