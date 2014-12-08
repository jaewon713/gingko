/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   host_info_util.h
 *
 * @author liuming03
 * @date   2013-9-29
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_HOST_INFO_UTIL_H_
#define OP_OPED_NOAH_TOOLS_BBTS_HOST_INFO_UTIL_H_

#include <netinet/in.h> // for in_addr_t
#include <sys/types.h>  // for pid_t, uid_t

#include <string>

namespace bbts {

const std::string& GetLocalHostname();

bool get_localhost_ip_addr(in_addr_t* ip_addr);

const std::string& GetLocalHostIPString();

bool get_local_machine_room(std::string *machine_room);

void trim_machine_room_digit(std::string *machine_room);

pid_t get_thread_id();

/**
 * @brief get a int random num
 */
int get_random_num();

/**
 * @brief get program uid for cache
 */
uid_t get_self_uid();

} // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_HOST_INFO_UTIL_H_
