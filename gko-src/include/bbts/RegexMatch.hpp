/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   RegexMatch.h
 *
 * @author liuming03
 * @date   2013-12-31
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_REGEX_MATCH_H_
#define OP_OPED_NOAH_TOOLS_BBTS_REGEX_MATCH_H_

#include <string>

#include <boost/regex.hpp>

namespace bbts {

/**
 * @brief
 */
class RegexMatch {
 public:
  explicit RegexMatch(const std::string &str) : str_(str) {}
  explicit RegexMatch(const char *str) : str_(str) {}

  virtual ~RegexMatch() {}

  inline bool operator () (const boost::regex &re) {
    return boost::regex_match(str_, re);
  }

 private:
  std::string str_;
};

} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_REGEX_MATCH_H_
