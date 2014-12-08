/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file   ThriftTracker.h
 *
 * @author liuming03
 * @date   2013-9-12
 * @brief 
 */

#ifndef OP_OPED_NOAH_TOOLS_BBTS_THRIFT_TRACKER_H_
#define OP_OPED_NOAH_TOOLS_BBTS_THRIFT_TRACKER_H_

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include "bbts/ConsistentHashRing.hpp"

namespace boost {
namespace asio {
class io_service;
}
}

namespace libtorrent {
class torrent_handle;
class ex_announce_request;
class ex_announce_response;
}

namespace bbts {
namespace message {
class TrackerArea;
}

namespace tracker {
class AnnounceRequest;
}

/**
 * @brief
 */
class ThriftTracker : private boost::noncopyable {
 public:
  typedef std::vector<std::pair<std::string, int> > TrackerVector;
  typedef boost::function<void(const boost::shared_ptr<libtorrent::ex_announce_response> &)> AnnounceCallback;

  ThriftTracker(boost::asio::io_service& ios);
  virtual ~ThriftTracker();

  inline bool HaveSeed() const {
    return have_seed_;
  }

  inline void SetAttribute(const std::string &peerid, const std::string &ip) {
    ip_ = ip;
    peerid_ = peerid;
  }

  inline void set_announce_callback(const AnnounceCallback &announce_callback) {
    announce_callback_ = announce_callback;
  }

  bool LoadThriftTrackers(const std::string &trackers_conf_path);

  void GetTrackersByInfohash(const std::string &infohash, TrackerVector *trackers) const;

  void ThriftTrackerAnnounce(const libtorrent::torrent_handle &handle,
                             boost::shared_ptr<libtorrent::ex_announce_request>);

 private:
  void LoadArea(const message::TrackerArea& area);

  void OnAnnounce(const libtorrent::torrent_handle &handle,
                  const TrackerVector &trackers,
                  const boost::shared_ptr<bbts::tracker::AnnounceRequest> &request) const;

  boost::asio::io_service& ios_;
  ConsistentHashRing trackers_hashring_[2];
  int current_trackers_hashring_index_;
  std::string ip_;
  std::string peerid_;
  std::string idc_;
  mutable bool have_seed_;
  AnnounceCallback announce_callback_;
};

}  // namespace bbts
#endif // OP_OPED_NOAH_TOOLS_BBTS_THRIFT_TRACKER_H_
