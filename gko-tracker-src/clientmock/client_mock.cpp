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

using std::string;
using boost::shared_ptr;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift;
using namespace bbts::tracker;

static void connect_server(int thread_id, int repead_id, string host, int port)
{
    char peerid[20];
    snprintf(peerid, 20, "%10d%10d", thread_id, repead_id);
    Peer peer;
    peer.__set_ip("192.168.1.1");
    peer.__set_idc("tc");
    peer.__set_port(1234);
    peer.__set_peerid(peerid);
    Stat stat;
    stat.__set_downloaded(thread_id * repead_id);
    stat.__set_left(thread_id + repead_id);
    stat.__set_uploaded(thread_id);
    stat.__set_status(static_cast<Status::type>(thread_id * repead_id % 5));
    AnnounceRequest request;
    request.__set_infohash(peerid);
    request.__set_is_seed(thread_id * repead_id % 2);
    request.__set_num_want(50);
    request.__set_peer(peer);
    request.__set_stat(stat);
    AnnounceResponse reponse;

    shared_ptr<TSocket> socket(new TSocket(host.c_str(), port));
    socket->setConnTimeout(3000);
    socket->setSendTimeout(3000);
    socket->setRecvTimeout(5000);
    shared_ptr<TTransport> transport(new TFramedTransport(socket));
    shared_ptr<TBinaryProtocol> protocol(new TBinaryProtocol(transport));
    AnnounceClient client(protocol);

    try {
        transport->open();
        client.announce(reponse, request);
        transport->close();
        //printf("success\n");
     } catch (TException &tx) {
       printf("ERROR: %s\n", tx.what());
     }
}

static void main_thread(int thread_id, int count, string host, int port)
{
    for (int i = 0; i < count; ++i) {
        connect_server(thread_id, i, host, port);
    }
}

int main(int argc, char* argv[])
{
    if (argc < 5) {
        printf("usage: %s host port thread repeat\n", argv[0]);
        return 0;
    }

    string host = argv[1];
    int port = atoi(argv[2]);
    int thread_num = atoi(argv[3]);
    int count = atoi(argv[4]);
    printf("%s:%d %d %d\n", host.c_str(), port, thread_num, count);

    boost::thread_group threads;
    for (int i = 0; i < thread_num; ++i) {
        threads.create_thread(boost::bind(&main_thread, i, count, host, port));
    }
    threads.join_all();
}


