/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   StopByInfohash.cpp
 *
 * @author liuming03
 * @date   2014-2-26
 * @brief 
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include "Announce.h"

using std::string;
using boost::shared_ptr;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift;
using namespace bbts::tracker;

inline static bool hexchar2digit(char c, uint8_t *digit)
                                 {
  assert(digit);
  if (c >= '0' && c <= '9') {
    *digit = c - '0';
  } else if (c >= 'a' && c <= 'f') {
    *digit = c - 'a' + 10;
  } else if (c >= 'A' && c <= 'F') {
    *digit = c - 'A' + 10;
  } else {
    return false;
  }
  return true;
}

bool hexstr2bytes(const std::string &hex, std::string* bytes)
                  {
  assert(bytes);
  std::string::size_type hex_len = hex.length();
  if (hex_len % 2 != 0) {
    return false;
  }

  bytes->clear();
  for (std::string::size_type i = 0; i < hex_len; i += 2) {
    uint8_t digit, byte;
    if (!hexchar2digit(hex[i], &digit)) {
      return false;
    }
    byte = digit << 4;
    if (!hexchar2digit(hex[i + 1], &digit)) {
      return false;
    }
    byte += digit;
    bytes->append(1, byte);
  }
  return true;
}

static void ControlByInfohash(const string &host, int port, const string &infohash, bool is_resume) {
  ControlByInfohashRequest request;
  request.__set_ip("192.168.1.1");// set my ip address
  request.__set_token("bbts-tracker-control");
  request.__set_infohash(infohash);

  shared_ptr<TSocket> socket(new TSocket(host.c_str(), port));
  socket->setConnTimeout(3000);
  socket->setSendTimeout(3000);
  socket->setRecvTimeout(5000);
  shared_ptr<TTransport> transport(new TFramedTransport(socket));
  shared_ptr<TBinaryProtocol> protocol(new TBinaryProtocol(transport));
  AnnounceClient client(protocol);

  try {
    transport->open();
    BaseResponse response;
    if (is_resume) {
      client.ResumeByInfohash(response, request);
    } else {
      client.StopByInfohash(response, request);
    }
    transport->close();
    printf("ret: %d, reason: %s\n", response.retval, response.message.c_str());
  } catch (TException &tx) {
    printf("ERROR: %s\n", tx.what());
  }
}

int main(int argc, char* argv[]) {
  if (argc < 4) {
    printf("usage: %s host port infohash is_resume\n", argv[0]);// TODO add control type
    return 0;
  }

  string host = argv[1];
  int port = atoi(argv[2]);
  std::string infohash = argv[3];
  bool is_resume = argc > 4 ? true : false;
  std::string output = is_resume ? "resume" : "pause";
  printf("%s from %s:%d by infohash %s is_resume: %d\n",
         output.c_str(), host.c_str(), port, infohash.c_str(), is_resume);
  std::string bytes_infohash;
  if (!hexstr2bytes(infohash, &bytes_infohash)) {
    printf("decode hex infohash fail\n");
    return -1;
  }
  ControlByInfohash(host, port, bytes_infohash, is_resume);
  return 0;
}

