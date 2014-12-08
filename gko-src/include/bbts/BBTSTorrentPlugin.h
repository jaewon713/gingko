/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file torrentplugin.h
 *
 * @author liuming03
 * @date 2013-7-31
 * @brief 
 */

#ifndef OP_OPED_NOAH_BBTS_TORRENT_PLUGIN_H_
#define OP_OPED_NOAH_BBTS_TORRENT_PLUGIN_H_

#include <boost/shared_ptr.hpp>
#include <libtorrent/address.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/extensions.hpp>

namespace libtorrent {
class torrent;
class peer_connection;
}

namespace bbts {

/**
 * @brief
 */

boost::shared_ptr<libtorrent::torrent_plugin>
create_bbts_torrent_plugin(libtorrent::torrent *torrent, void *args);

class BBTSTorrentPlugin : public libtorrent::torrent_plugin {
public:
  BBTSTorrentPlugin(libtorrent::torrent& t);

  virtual ~BBTSTorrentPlugin();

  virtual boost::shared_ptr<libtorrent::peer_plugin> new_connection(libtorrent::peer_connection* pc);

private:
  libtorrent::torrent& torrent_;
};

class BBTSTorrentPeerPlugin : public libtorrent::peer_plugin {
public:
  BBTSTorrentPeerPlugin(libtorrent::peer_connection& pc, libtorrent::torrent& t);
  virtual ~BBTSTorrentPeerPlugin();

  virtual bool on_handshake(char const* reserved_bits);

private:
  time_t start_time_;
  libtorrent::peer_connection& peer_connection_;
  libtorrent::sha1_hash infohash_;
  libtorrent::aux::session_impl &session_;
};

struct peer_stat_alert : libtorrent::alert {
  peer_stat_alert(const libtorrent::sha1_hash &infohash,
                  const std::string &remote_ip,
                  time_t start_time,
                  time_t end_time,
                  int64_t uploaded,
                  int64_t downloaded,
                  bool is_hdfs)
    : infohash_(infohash),
      remote_ip_(remote_ip),
      start_time_(start_time),
      end_time_(end_time),
      uploaded_(uploaded),
      downloaded_(downloaded),
      is_hdfs_(is_hdfs) {}

  virtual ~peer_stat_alert() {}

  virtual int type() const {
    return alert_type;
  }

  virtual std::auto_ptr<alert> clone() const {
    return std::auto_ptr<alert>(new peer_stat_alert(*this));
  }

  virtual int category() const {
    return static_category;
  }

  virtual char const* what() const {
    return "peer_stat_alert";
  }

  virtual std::string message() const;

  libtorrent::sha1_hash infohash_;
  std::string remote_ip_;
  time_t start_time_;
  time_t end_time_;
  int64_t uploaded_;
  int64_t downloaded_;
  bool is_hdfs_;
  const static int static_category = libtorrent::alert::peer_notification;
  const static int alert_type = libtorrent::user_alert_id + 1;
};

struct peer_handshake_alert : libtorrent::alert {
  peer_handshake_alert(const libtorrent::sha1_hash &infohash,
                       const std::string &remote_ip,
                       int remote_port)
    : infohash_(infohash),
      remote_ip_(remote_ip),
      remote_port_(remote_port) {}

  virtual ~peer_handshake_alert() {}

  virtual int type() const {
    return alert_type;
  }

  virtual std::auto_ptr<alert> clone() const {
    return std::auto_ptr<alert>(new peer_handshake_alert(*this));
  }

  virtual int category() const {
    return static_category;
  }

  virtual char const* what() const {
    return "peer_handshake_alert";
  }

  virtual std::string message() const;

  libtorrent::sha1_hash infohash_;
  std::string remote_ip_;
  int remote_port_;
  const static int static_category = libtorrent::alert::peer_notification;
  const static int alert_type = libtorrent::user_alert_id + 2;
};

}  // namespace bbts

#endif // OP_OPED_NOAH_BBTS_TORRENT_PLUGIN_H_
