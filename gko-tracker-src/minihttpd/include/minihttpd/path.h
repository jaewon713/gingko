#ifndef ARGUS_COMMON_PATH_H_
#define ARGUS_COMMON_PATH_H_

#include "types.h"
#include <string.h>

namespace argus {
namespace common {
namespace Path {

inline bool isSeparator(char ch) {
  return ch == '/';
}

string getBaseName(const char* filepath) {
  size_t len = strlen(filepath);
  int i = static_cast<int>(len - 1);
  for ( ; i >= 0; i--) {
    if (isSeparator(filepath[i]))
      break;
  }
  return string(filepath + i + 1, filepath + len);
}

string getBaseName(const string& filepath) {
  return getBaseName(filepath.c_str());
}

string getDirectory(const char* filepath) {
  size_t len = strlen(filepath);
  int i = static_cast<int>(len - 1);
  for ( ; i >= 0; i--) {
    if (isSeparator(filepath[i]))
      break;
  }
  if (i >= 0)
    return string(filepath, filepath + i + 1);
  return "";
}

string getDirectory(const string& filepath) {
  return getDirectory(filepath.c_str());
}


} // namespace Path
} // namespace common
} // namespace argus

#endif  // ARGUS_COMMON_PATH_H_

