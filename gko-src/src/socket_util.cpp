/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   socket_util.cpp
 *
 * @author liuming03
 * @date   2013-12-3
 * @brief 
 */

#include "bbts/socket_util.h"

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <vector>

#include "bbts/constant_def.h"
#include "bbts/log.h"
#include "bbts/UnixSocketConnection.h"

using std::vector;

using boost::shared_ptr;

namespace bbts {

bool WriteMessage(const shared_ptr<UnixSocketConnection> &connection,
                  uint32_t type,
                  const google::protobuf::Message &response) {
  shared_ptr<vector<char> > data(new vector<char>(response.ByteSize() + sizeof(type)));
  char *ptr = &(*data)[0];
  *reinterpret_cast<uint32_t *>(ptr) = type;
  if (!response.SerializeToArray(ptr + sizeof(type), data->size() - sizeof(type))) {
    WARNING_LOG("serialize message to string failed.");
    return false;
  }
  connection->Write(data);
  return true;
}

bool send_cred(int fd) {
  char c = 255;

  struct iovec iov;
  iov.iov_base = &c;
  iov.iov_len = 1;

  union {
    struct cmsghdr cm;
    char control[CMSG_SPACE(sizeof(struct ucred))];
  } control_un;

  struct msghdr msg;
  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = &control_un.control;
  msg.msg_controllen = sizeof(control_un.control);
  msg.msg_flags = 0;

  struct cmsghdr * cmptr = CMSG_FIRSTHDR(&msg);
  cmptr->cmsg_len = CMSG_LEN(sizeof(struct ucred));
  cmptr->cmsg_level = SOL_SOCKET;
  cmptr->cmsg_type = SCM_CREDENTIALS;
  struct ucred *ucptr = (struct ucred *)CMSG_DATA(cmptr);
  ucptr->pid = getpid();
  ucptr->uid = getuid();
  ucptr->gid = getgid();

  int n = sendmsg(fd, &msg, 0);
  if (n != 1) {
    return false;
  }
  return true;
}

bool recv_cred(int fd, struct ucred *ucptr) {
  int on = 1;
  int ret = setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));
  if (ret < 0) {
    return false;
  }

  char c;
  struct iovec iov;
  iov.iov_base = &c;  //数据
  iov.iov_len = 1;

  union {
    struct cmsghdr cm;
    char control[CMSG_SPACE(sizeof(struct ucred))];
  } control_un;

  struct msghdr msg;
  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control_un.control;
  msg.msg_controllen = sizeof(control_un.control);
  msg.msg_flags = 0;

  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = POLLIN;
  ret = poll(&pfd, 1, 1000);
  if (ret <= 0) {  //超时未能读取则算失败，agent会close该fd
    return false;
  }

  int n = recvmsg(fd, &msg, 0);
  if (n != 1) {
    return false;
  }

  if (msg.msg_controllen < sizeof(struct cmsghdr)) {
    return false;
  }

  struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
  if (cmptr->cmsg_len != CMSG_LEN(sizeof(struct ucred))) {
    return false;
  }

  if (cmptr->cmsg_level != SOL_SOCKET || cmptr->cmsg_type != SCM_CREDENTIALS) {
    return false;
  }

  struct ucred * tmp = (struct ucred *)CMSG_DATA(cmptr);
  memcpy(ucptr, tmp, sizeof(struct ucred));
  return true;
}

} // namespace bbts
