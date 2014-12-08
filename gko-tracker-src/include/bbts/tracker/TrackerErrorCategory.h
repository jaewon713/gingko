/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   TrackerErrorCategory.h
 *
 * @author liuming03
 * @date   2014-2-26
 * @brief 
 */

#ifndef OP_OPED_NOAH_BBTS_TRACKER_ERROR_CATEGORY_H_
#define OP_OPED_NOAH_BBTS_TRACKER_ERROR_CATEGORY_H_

#include <boost/system/error_code.hpp>

namespace bbts {
namespace tracker {

namespace errors {
/* error code */
enum error_code_t {
  NO_ERROR = 0,
  MISSING_ARGS,
  INVALID_ARGS,
  INFOHASH_NOT_FOUND,
  INVALID_OPERAT_TYPE,
  MAX_ERROR_NUM,
};

}

/**
 * @brief
 */
class TrackerErrorCategory : public boost::system::error_category {
 public:
  TrackerErrorCategory() {}
  virtual ~TrackerErrorCategory(){}
  virtual const char* name() const {
    return name_;
  }

  virtual std::string message(int ev) const;

 private:
  static const char *name_;
};

const boost::system::error_category& get_error_category();

}  // namespace tracker
}  // namespace bbts
#endif // OP_OPED_NOAH_BBTS_TRACKER_ERROR_CATEGORY_H_
