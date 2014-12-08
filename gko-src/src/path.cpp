/***************************************************************************
 *
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/

/**
 * @file   path.cpp
 *
 * @author liuming03
 * @date   2013-4-9
 */

#include "bbts/path.h"

#include <stdlib.h>
#include <sys/stat.h>

#include <string>
#include <vector>

using std::string;

namespace bbts {

bool get_self_path(string *self_path) {
  assert(self_path);
  char buf[MAX_PATH_LEN] = { 0 };
  if (readlink("/proc/self/exe", buf, sizeof(buf)) < 0) {
    return false;
  }
  self_path->assign(buf);
  return true;
}

bool get_current_working_dir(string *cwd) {
  assert(cwd);
  char path[MAX_PATH_LEN] = { 0 };
  char *ptr = getcwd(path, MAX_PATH_LEN);
  if (!ptr) {
    return false;
  }
  cwd->assign(ptr);
  return true;
}

string trim_back_slash(const string &path) {
  string::size_type pos = path.length() - 1;
  while (pos != string::npos && path[pos] == '/') {
    --pos;
  }
  return path.substr(0, pos + 1);
}

string trim_front_slash(const string &path) {
  string::size_type pos = 0;
  while (pos < path.length() && path[pos] == '/') {
    ++pos;
  }
  return path.substr(pos);
}

bool path_slipt(const string &path, string *parent_path, string *short_name) {
  assert(parent_path && short_name);
  string tmp_path = trim_back_slash(path);
  string::size_type pos = tmp_path.rfind('/');
  if (pos != string::npos && pos != tmp_path.length() - 1) {
    *parent_path = 0 == pos ? "/" : tmp_path.substr(0, pos);
    short_name->assign(tmp_path.substr(pos + 1));
    return true;
  }
  return false;
}

string parent_path(const string &path) {
  string tmp_path = trim_back_slash(path);
  string::size_type pos = tmp_path.rfind('/');
  if (pos != string::npos) {
    return 0 == pos ? "/" : tmp_path.substr(0, pos);
  }
  return string();
}

string file_name(const string &path) {
  string tmp_path = trim_back_slash(path);
  string::size_type pos = tmp_path.rfind('/');
  if (pos != string::npos) {
    return tmp_path.substr(pos + 1);
  } else {
    return tmp_path;
  }
}

string subpath(const string &path) {
  string tmp_path = trim_front_slash(path);
  string::size_type pos = tmp_path.find('/');
  if (pos != string::npos) {
    return tmp_path.substr(pos + 1);
  } else {
    return string();
  }
}

bool get_home_path(string *home_path) {
  assert(home_path);
  char *path = getenv("HOME");
  if (!path) {
    return false;
  }
  home_path->assign(path);
  return true;
}

bool get_root_path(string *root_path) {
  assert(root_path);
  string program;
  if (!get_self_path(&program)) {
    return false;
  }
  root_path->assign(parent_path(parent_path(program)));
  if (root_path->empty()) {
    return false;
  }
  return true;
}

string complete_full_path(const string &path) {
  string full_path = path;
  if (path.empty() || path[0] != '/') {
    string cwd;
    bool ret = get_current_working_dir(&cwd);
    assert(ret);
    full_path = cwd + '/' + full_path;
  }
  return full_path;
}

string insert_back_slash(const string &path) {
  string tmp;
  string::size_type start = 0;
  string::size_type pos;
  while ((pos = path.find('/', start)) != string::npos) {
    tmp.append(path.substr(start, pos - start)).append("\\/");
    start = pos + 1;
  }
  tmp.append(path.substr(start));
  return tmp;
}

bool is_absolute_path(const string &path) {
  if (path[0] == '/') {
    return true;
  }
  return false;
}

string trim_path(const string &path) {
  if (path.empty()) {
    return path;
  }
  std::vector<string> v;
  if (path[0] == '/') {
    v.push_back(string("/"));
  }
  string tmp;
  string::size_type start = 0;
  string::size_type pos;
  do {
    pos = path.find('/', start);
    tmp = path.substr(start, pos - start);
    if (tmp.empty() || tmp == ".") {
    } else if (tmp == "..") {
      if (v.size() <= 1) {
        return string();
      }
      v.pop_back();
    } else {
      v.push_back(tmp);
    }
    start = pos + 1;
  } while (pos != string::npos);
  assert(!v.empty());
  tmp = v[0] == "/" ? "" : v[0];
  size_t v_len = v.size();
  for (size_t i = 1; i != v_len; ++i) {
    tmp.append("/").append(v[i]);
  }
  return tmp;
}

bool check_and_mkdir(const string &path, mode_t mode) {
  struct stat statbuf;
  if (stat(path.c_str(), &statbuf) != 0) {  // 没有该路径则创建
    if (mkdir(path.c_str(), mode) != 0) {
      return false;
    }
  } else if (!(statbuf.st_mode & S_IFDIR)) {
    return false;
  }
  return true;
}

bool check_and_mkdir_r(const string &path, mode_t mode) {
  string parent = parent_path(path);
  if (!parent.empty()) {
    struct stat statbuf;
    if (stat(parent.c_str(), &statbuf) != 0) {
      if (!check_and_mkdir_r(parent, mode)) {
        return false;
      }
    } else if (!(statbuf.st_mode & S_IFDIR)) {
      return false;
    }
  }
  return check_and_mkdir(path, mode);
}

} // namespace bbts
