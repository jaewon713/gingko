/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   ThriftTracker.cpp
 *
 * @author liuming03
 * @date   2013-9-12
 * @brief 
 */

#include "bbts/ThriftTracker.h"

#include <netinet/in.h>
#include <sys/socket.h>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <libtorrent/torrent_handle.hpp>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/Thrift.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>

#include "bbts/host_info_util.h"
#include "bbts/log.h"
#include "bbts/pb_conf.h"
#include "bbts/tracker/Announce.h"
#include "bbts/TrackerConf.pb.h"

using std::string;
using std::vector;
using boost::asio::io_service;
using boost::shared_ptr;
using bbts::tracker::AnnounceRequest;
using bbts::tracker::AnnounceResponse;
using bbts::tracker::Status;
using libtorrent::ex_announce_request;
using libtorrent::ex_announce_response;
using libtorrent::torrent_handle;

namespace bbts {

static void DefaultAnnounceCallback(const shared_ptr<ex_announce_response> &) {}

ThriftTracker::ThriftTracker(io_service& ios) :
    ios_(ios),
    current_trackers_hashring_index_(0),
    have_seed_(false),
    announce_callback_(boost::bind(&DefaultAnnounceCallback, _1)) {}

ThriftTracker::~ThriftTracker() {}

void ThriftTracker::LoadArea(const message::TrackerArea& area) {
  char buf[256] = { '\0' };
  for (int i = 0; i < area.tracker_size(); ++i) {
    const message::Tracker &tracker = area.tracker(i);
    snprintf(buf, sizeof(buf), "%s %d", tracker.host().c_str(), tracker.port());
    trackers_hashring_[!current_trackers_hashring_index_].AddNode(string(buf));
  }
}

bool ThriftTracker::LoadThriftTrackers(const string &trackers_conf_path) {
  trackers_hashring_[!current_trackers_hashring_index_].Clear();
  if (!get_local_machine_room(&idc_)) {
    WARNING_LOG("can't know machine room of this machine, will use default.");
  }

  string machine_room_without_digit = idc_;
  trim_machine_room_digit(&machine_room_without_digit);
  message::TrackerConf tracker_conf;
  if (!LoadPBConf(trackers_conf_path, &tracker_conf)) {
    FATAL_LOG("load tracker conf(%s) failed", trackers_conf_path.c_str());
    return false;
  }

  bool use_default = true;
  for (int j = 0; j < tracker_conf.area_size(); ++j) {
    const message::TrackerArea& area = tracker_conf.area(j);
    bool find_area = false;
    for (int i = 0; i < area.machine_room_size(); ++i) {
      if (machine_room_without_digit == area.machine_room(i)) {
        find_area = true;
        break;
      }
    }
    if (find_area) {
      LoadArea(area);
      use_default = false;
      break;
    }
  }

  if (use_default) {
    const message::TrackerArea& area = tracker_conf.default_();
    LoadArea(area);
  }

  if (trackers_hashring_[!current_trackers_hashring_index_].empty()) {
    FATAL_LOG("can't find a thrift tracker to use for announce!");
    return false;
  }
  current_trackers_hashring_index_ = !current_trackers_hashring_index_;
  return true;
}

void ThriftTracker::GetTrackersByInfohash(const string &infohash, TrackerVector *trackers) const {
  vector<string> tracker_nodes;
  trackers_hashring_[current_trackers_hashring_index_].GetAllNodes(infohash, &tracker_nodes);
  for(vector<string>::const_iterator it = tracker_nodes.begin(); it != tracker_nodes.end(); ++it) {
    string ip;
    uint16_t port;
    std::stringstream strm(*it);
    strm >> ip >> port;
    trackers->push_back(std::make_pair(ip, port));
  }
}

void ThriftTracker::ThriftTrackerAnnounce(const torrent_handle &handle,
                                          shared_ptr<ex_announce_request> req) {
  shared_ptr<AnnounceRequest> request(new AnnounceRequest());
  request->__set_infohash(req->infohash);
  request->peer.__set_ip(req->peer.ip);
  request->peer.__set_port(req->peer.port);
  request->peer.__set_peerid(req->peer.peerid);
  request->peer.__set_idc(idc_);
  request->__isset.peer = true;
  request->stat.__set_uploaded(req->uploaded);
  request->stat.__set_downloaded(req->downloaded);
  request->stat.__set_left(req->left);
  request->__isset.stat = true;
  request->__set_is_seed(req->is_seed);
  request->__set_num_want(req->num_want);
  switch (req->status) {
    case ex_announce_request::DOWNLOAD:
      request->stat.__set_status(Status::DOWNLOAD);
      break;

    case ex_announce_request::METADATA:
      request->stat.__set_status(Status::METADATA);
      break;

    case ex_announce_request::SEEDING:
      request->stat.__set_status(Status::SEEDING);
      break;

    case ex_announce_request::PAUSED:
      case ex_announce_request::STOPPED:
      request->stat.__set_status(Status::STOPPED);
      break;
  }
  // libtorrent 内置主线程，不要堵塞其，发送给我们自己的线程来处理announce
  ios_.post(boost::bind(&ThriftTracker::OnAnnounce, this, handle, req->trackers, request));
}

void ThriftTracker::OnAnnounce(const torrent_handle &handle,
                               const TrackerVector &trackers,
                               const shared_ptr<AnnounceRequest> &request) const {
  using apache::thrift::protocol::TBinaryProtocol;
  using apache::thrift::transport::TSocket;
  using apache::thrift::transport::TFramedTransport;
  using apache::thrift::TException;
  using bbts::tracker::Peer;
  using bbts::tracker::AnnounceClient;
  using libtorrent::ex_announce_peer;

  TRACE_LOG("[%s][seed:%d][state:%d][uploaded:%lld][downloaded:%lld][left:%lld][num_want:%d]",
      libtorrent::to_hex(request->infohash).c_str(), request->is_seed, request->stat.status, request->stat.uploaded,
      request->stat.downloaded, request->stat.left, request->num_want);

  shared_ptr<AnnounceResponse> response(new AnnounceResponse());
  bool announce_success = false;
  std::stringstream strm;
  for (TrackerVector::const_iterator it = trackers.begin(); it != trackers.end(); ++it) {
    shared_ptr<TSocket> socket(new TSocket(it->first, it->second));
    socket->setConnTimeout(3000);
    socket->setSendTimeout(3000);
    socket->setRecvTimeout(5000);
    shared_ptr<TFramedTransport> transport(new TFramedTransport(socket));
    shared_ptr<TBinaryProtocol> protocol(new TBinaryProtocol(transport));
    AnnounceClient client(protocol);
    try {
      transport->open();
      client.announce((*response.get()), (*request.get()));
      transport->close();
      announce_success = true;
      if (response->ret == 0) {
        TRACE_LOG("[tracker:%s:%d][recv:%d][have_seed:%d]",
                   it->first.c_str(), it->second, response->peers.size(), response->have_seed);
      } else {
        WARNING_LOG("[tracker:%s:%d][fail:%s]",
                     it->first.c_str(), it->second, response->failure_reason.c_str());
      }
      break;
    } catch (TException &tx) {
      WARNING_LOG("[tracker:%s:%d][fail:%s]", it->first.c_str(), it->second, tx.what());
    }
  }

  shared_ptr<ex_announce_response> res(new ex_announce_response);
  if (!announce_success) {
    res->ret = 10000;
    res->failure_reason = "announce to tracker failed";
  } else {
    res->ret = response->ret;
    res->failure_reason = response->failure_reason;
    res->have_seed = have_seed_ = response->have_seed;
    res->min_interval = response->min_interval;
    res->peers.resize(response->peers.size());
    vector<Peer>::iterator it = response->peers.begin();
    for (int i = 0; it != response->peers.end(); ++it, ++i) {
      ex_announce_peer &peer = res->peers[i];
      peer.ip = it->ip;
      peer.peerid = it->peerid;
      peer.port = it->port;
    }
  }
  announce_callback_(res);
  handle.recv_ex_tracker_announce_reply(res);
}

} // namespace bbts
