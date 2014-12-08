/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   ErrorCategory.h
 *
 * @author liuming03
 * @date   2014-1-15
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_ERROR_CATEGORY_H_
#define OP_OPED_NOAH_TOOLS_BBTS_ERROR_CATEGORY_H_

#include <boost/system/error_code.hpp>

namespace bbts {

namespace errors {
/* error code */
enum error_code_t {
  NO_ERROR = 0,
  MEM_ALLOC_ERROR,
  MSG_READ_ERROR,
  PROTOBUF_PARSE_ERROR,
  MSG_ARGS_ERROR,
  TORRENT_FILE_ERROR,
  INFOHASH_NOT_CORRECT,
  DUP_TASK_ERROR,
  GENERATE_TASK_ID_FAILED,
  ADD_TORRENT_ERROR,
  TASK_NOT_FOUND,
  TRACKER_ERROR,
  READ_CRED_FAILED,
  CHECK_CRED_FAILED,
  DB_ERROR,
  NO_METADATA,
  PARSE_METADATA_FAILED,
  MAX_ERROR_NUM,
};

}

/**
 * @brief
 */
class ErrorCategory : public boost::system::error_category {
public:
  ErrorCategory() {}
  virtual ~ErrorCategory(){}
  virtual const char* name() const {
    return name_;
  }

  virtual std::string message(int ev) const;

private:
  static const char *name_;
};

const boost::system::error_category& get_error_category();

} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_ERROR_CATEGORY_H_
