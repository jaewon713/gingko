#ifndef ARGUS_COMMON_STRING_ALGOS_H_
#define ARGUS_COMMON_STRING_ALGOS_H_

#include "common.h"
#include <ctype.h>

namespace argus {
namespace common {

inline void StringToLower(string* s) {
  string::iterator end = s->end();
  for (string::iterator i = s->begin(); i != end; ++i){
    *i = ::tolower(static_cast<int>(*i));
  }
}

inline void StringTrimLeft(string* str) {
  size_t start_pos = 0;
  size_t end_pos = str->length();
  while (start_pos != end_pos && ::isspace(str->at(start_pos))) {
    start_pos++;
  }
  *str = str->substr(start_pos);
}

inline void StringTrimRight(string* str) {
  int end_pos = static_cast<int>(str->length()) - 1;
  while (end_pos >= 0 && ::isspace(str->at(end_pos))) {
    end_pos--;
  }
  *str = str->substr(0, end_pos + 1);
}

inline void StringTrim(string* str) {
  size_t start_pos = 0;
  size_t end_pos = str->length();
  while (start_pos != end_pos && ::isspace(str->at(start_pos))) {
    start_pos++;
  }

  if (start_pos == end_pos) {
      str->clear();
      return;
  }

  end_pos--;

  // end_pos always >= 0
  while (::isspace(str->at(end_pos))) {
    end_pos--;
  }
  *str = str->substr(start_pos, end_pos - start_pos + 1);
}

}  // namespace common
}  // namespace argus

#endif // ARGUS_COMMON_STRING_ALGOS_H_

