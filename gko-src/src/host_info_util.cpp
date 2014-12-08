/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   host_info_util.cpp
 *
 * @author liuming03
 * @date   2013-9-29
 * @brief 
 */

#include "bbts/host_info_util.h"

#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <unistd.h>

using std::string;

namespace bbts {

const string& GetLocalHostname() {
  static string hostname;
  if (hostname.empty()) {
    char buf[256] = { 0 };
    if (gethostname(buf, sizeof(buf)) == 0) {
      hostname = buf; // notice critical area for not threaded safety
    }
  }
  return hostname;
}

bool get_localhost_ip_addr(in_addr_t* ip_addr) {
  assert(ip_addr);

  int h_err;
  char buf[8096] = { 0 };
  struct hostent ent, *ptr;
  int ret = gethostbyname_r(GetLocalHostname().c_str(), &ent, buf, sizeof(buf), &ptr, &h_err);
  if (ret != 0 || h_err != 0) {
    fprintf(stderr, "Get host by name(%s) failed: %s", GetLocalHostname().c_str(), hstrerror(h_err));
    return false;
  }
  *ip_addr = *(in_addr_t *)(ent.h_addr);
  return true;
}

static in_addr_t get_static_local_ip_addr() {
  in_addr_t ip_addr = 0;
  get_localhost_ip_addr(&ip_addr);
  return ip_addr;
}

const string& GetLocalHostIPString() {
  static string ip;
  if (ip.empty()) {
    in_addr_t ip_addr;
    if (get_localhost_ip_addr(&ip_addr)) {
      unsigned char *p = (unsigned char *)(&ip_addr);
      char ip_str[16];
      snprintf(ip_str, sizeof(ip_str), "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
      ip = ip_str;
    }
  }
  return ip;
}

pid_t get_thread_id() {
  pid_t tid = -1;
#if defined(__linux__)
  tid = syscall(SYS_gettid);
#endif
  return tid;
}

int get_random_num() {
  static in_addr_t ip_addr = get_static_local_ip_addr();
  struct timeval tv;
  gettimeofday(&tv, NULL);
  unsigned randseed = (unsigned)(ip_addr) ^ (unsigned)(get_thread_id())
      ^ (unsigned)(tv.tv_sec) ^ (unsigned)(tv.tv_usec);
  return rand_r(&randseed);
}

bool get_local_machine_room(string *machine_room) {
  assert(machine_room);
  string hostname = GetLocalHostname();
  if (hostname.empty()) {
    return false;
  }
  string::size_type index = hostname.rfind(".baidu.com");
  if (index != string::npos) {
    hostname.erase(index);
  }

  index = hostname.rfind(".");
  if (index != string::npos) {
    machine_room->assign(hostname, index + 1, hostname.length() - index - 1);
  } else {
    return false;
  }

  if (*machine_room == "vm") {
    index = hostname.find("-");
    if (index != string::npos) {
      machine_room->assign(hostname, 0, index);
    } else {
      machine_room->clear();
      return false;
    }
  }
  return true;
}

void trim_machine_room_digit(string *machine_room) {
  assert(machine_room);
  string::size_type index;
  for (index = machine_room->size() - 1; index != string::npos; --index) {
    char &letter = machine_room->at(index);
    if (letter < '0' || letter > '9') {
      break;
    }
  }

  ++index;
  if (index < machine_room->size() - 1) {
    machine_room->erase(index);
  }
}

uid_t get_self_uid() {
  static uid_t uid = getuid();
  return uid;
}

} // namespace bbts
