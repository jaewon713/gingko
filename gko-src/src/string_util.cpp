/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   string_util.cpp
 *
 * @author liuming03
 * @date   2013-12-30
 * @brief 
 */

#include "bbts/string_util.h"

#include <assert.h>

using std::string;
using std::vector;
using std::set;

namespace bbts {

void SliptStringToVector(const string &str, const string &delimiter, vector<string> *v) {
  assert(v);
  if (str.empty()) {
    return;
  }
  v->clear();
  string::size_type start_pos(0), end_pos(string::npos);
  while ((end_pos = str.find_first_of(delimiter, start_pos)) != string::npos) {
    if (end_pos != start_pos) {
      v->push_back(str.substr(start_pos, end_pos - start_pos));
    }
    start_pos = end_pos + 1;
  }
  if (start_pos < str.length()) {
    v->push_back(str.substr(start_pos));
  }
}

void SliptStringToSet(const string &str, const string &delimiter, set<string> *s) {
  assert(s);
  if (str.empty()) {
    return;
  }
  s->clear();
  string::size_type start_pos(0), end_pos(string::npos);
  while ((end_pos = str.find_first_of(delimiter, start_pos)) != string::npos) {
    if (end_pos != start_pos) {
      s->insert(str.substr(start_pos, end_pos - start_pos));
    }
    start_pos = end_pos + 1;
  }
  if (start_pos < str.length()) {
    s->insert(str.substr(start_pos));
  }
}

} // namespace bbts
