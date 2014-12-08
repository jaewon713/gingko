/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file client_mock.cpp
 *
 * @author liuming03
 * @date 2013-9-11
 * @brief 
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include "Announce.h"
#include "bbts/encode.h"

using std::string;
using std::vector;
using boost::shared_ptr;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift;
using namespace bbts::tracker;

inline static bool HexcharToDigit(char c, uint8_t *digit) {
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

bool HexstrToBytes(const string &hex, string* bytes) {
  assert(bytes);
  string::size_type hex_len = hex.length();
  if (hex_len % 2 != 0) {
    return false;
  }

  bytes->clear();
  for (string::size_type i = 0; i < hex_len; i += 2) {
    uint8_t digit, byte;
    if (!HexcharToDigit(hex[i], &digit)) {
      return false;
    }
    byte = digit << 4;
    if (!HexcharToDigit(hex[i + 1], &digit)) {
      return false;
    }
    byte += digit;
    bytes->append(1, byte);
  }
  return true;
}

inline static char DigitToHex(uint8_t digit) {
  static const char hexchars[] = {
      '0', '1', '2', '3', '4', '5', '6', '7',
      '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
  };
  return hexchars[digit & 0x0f];
}

bool BytesToHex(const string &bytes, string* hex) {
  assert(hex);
  hex->clear();
  string::size_type bytes_len = bytes.length();
  for (string::size_type i = 0; i < bytes_len; ++i) {
    uint8_t byte = bytes[i];
    char c = DigitToHex((byte & 0xf0) >> 4);
    hex->append(1, c);
    c = DigitToHex(byte & 0x0f);
    hex->append(1, c);
  }
  return true;
}

static void connect_server(string host, int port, string infohash, string peerid, int status,
                           bool is_seed) {
  Peer peer;
  peer.__set_ip("10.38.75.54");
  peer.__set_idc("hz");
  peer.__set_port(422);
  peer.__set_peerid(peerid);
  Stat stat;
  stat.__set_downloaded(0);
  stat.__set_left(100);
  stat.__set_uploaded(0);
  stat.__set_status(static_cast<Status::type>(status));
  AnnounceRequest request;
  request.__set_infohash(infohash);
  request.__set_is_seed(is_seed);
  request.__set_num_want(10);
  request.__set_peer(peer);
  request.__set_stat(stat);
  AnnounceResponse response;

  shared_ptr<TSocket> socket(new TSocket(host.c_str(), port));
  socket->setConnTimeout(3000);
  socket->setSendTimeout(3000);
  socket->setRecvTimeout(5000);
  shared_ptr<TTransport> transport(new TFramedTransport(socket));
  shared_ptr<TBinaryProtocol> protocol(new TBinaryProtocol(transport));
  AnnounceClient client(protocol);

  try {
    transport->open();
    client.announce(response, request);
    transport->close();
    printf("ret: %d, reason: %s\n", response.ret, response.failure_reason.c_str());
    printf("min_interval: %d, have_seed: %d, peers_num: %ld\n", response.min_interval,
           response.have_seed,
           response.peers.size());
    for (vector<Peer>::iterator it = response.peers.begin(); it != response.peers.end(); ++it) {
      string peerid;
      BytesToHex(it->peerid, &peerid);
      printf("%s:%d %s\n", it->ip.c_str(), it->port, peerid.c_str());
    }
  } catch (TException &tx) {
    printf("ERROR: %s\n", tx.what());
  }
}

int main(int argc, char* argv[]) {
  if (argc < 6) {
    printf("usage: %s host port infohash peerid status [is_seed]\n", argv[0]);
    return 0;
  }

  string host = argv[1];
  int port = atoi(argv[2]);
  std::string infohash = argv[3];
  std::string peerid = argv[4];
  int status = atoi(argv[5]);
  bool is_seed = argc > 6 ? true : false;
  printf("%s:%d %s %s is_seed:%d, status: %d\n", host.c_str(), port, infohash.c_str(),
         peerid.c_str(),
         is_seed,
         status);
  std::string bytes_infohash, bytes_peerid;
  if (!HexstrToBytes(infohash, &bytes_infohash)) {
    printf("decode hex infohash fail\n");
    return -1;
  }
  if (!HexstrToBytes(peerid, &bytes_peerid)) {
    printf("decode hex peerid fail\n");
    return -1;
  }
  connect_server(host, port, bytes_infohash, bytes_peerid, status, is_seed);
  return 0;
}

