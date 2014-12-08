/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   socket_util.h
 *
 * @author liuming03
 * @date   2013-12-3
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_SOCKET_UTIL_H_
#define OP_OPED_NOAH_TOOLS_BBTS_SOCKET_UTIL_H_

#include <boost/shared_ptr.hpp>
#include <google/protobuf/message.h>

struct ucred;

namespace bbts {

class UnixSocketConnection;

bool WriteMessage(const boost::shared_ptr<UnixSocketConnection> &connection,
                  uint32_t type,
                  const google::protobuf::Message &response);

/**
 * @brief 发送认证信息给对端
 *
 * @param fd          已连接的unix sock
 * @return          成功返回0，失败返回-1
 */
bool send_cred(int fd);

/**
 * @brief 从对端接收认证信息
 *
 * @param fd          已连接的unix sock
 * @return          成功返回0，失败返回-1
 */
bool recv_cred(int fd, struct ucred *ucptr);

} // namespace bbts

#endif // OP_OPED_NOAH_TOOLS_BBTS_SOCKET_UTIL_H_
