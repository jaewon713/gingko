/***************************************************************************
 * 
 * Copyright (c) 2013 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file torrentplugin.cpp
 *
 * @author liuming03
 * @date 2013-7-31
 * @brief 
 */

#include "bbts/BBTSTorrentPlugin.h"

#include <libtorrent/bt_peer_connection.hpp>
#include <libtorrent/peer_connection.hpp>
#include <libtorrent/peer_info.hpp>
#include <libtorrent/torrent.hpp>

#include "bbts/host_info_util.h"

using boost::shared_ptr;
using boost::system::error_code;
using libtorrent::peer_connection;
using libtorrent::peer_plugin;
using libtorrent::tcp;
using libtorrent::torrent;
using libtorrent::torrent_plugin;

namespace bbts {

shared_ptr<torrent_plugin> create_bbts_torrent_plugin(torrent *t, void *args) {
  return shared_ptr<torrent_plugin>(new BBTSTorrentPlugin(*t));
}

BBTSTorrentPlugin::BBTSTorrentPlugin(libtorrent::torrent& t) : torrent_(t) {}

BBTSTorrentPlugin::~BBTSTorrentPlugin() {}

shared_ptr<peer_plugin> BBTSTorrentPlugin::new_connection(peer_connection *pc) {
  if (pc->type() != peer_connection::bittorrent_connection) {
    return shared_ptr<peer_plugin>();
  }
  return shared_ptr<peer_plugin>(new BBTSTorrentPeerPlugin(*pc, torrent_));
}

std::string peer_stat_alert::message() const {
  char ih_hex[41];
  libtorrent::to_hex((const char*)&infohash_[0], 20, ih_hex);
  error_code ec;
  char msg[200];
  snprintf(msg, sizeof(msg), "%s,%s,%s,%ld,%ld,%ld,%ld,%d", ih_hex, GetLocalHostIPString().c_str(),
           remote_ip_.c_str(), start_time_, end_time_, uploaded_, downloaded_, is_hdfs_);
  return msg;
}

std::string peer_handshake_alert::message() const {
  char ih_hex[41];
  libtorrent::to_hex((const char*)&infohash_[0], 20, ih_hex);
  error_code ec;
  char msg[200];
  snprintf(msg, sizeof(msg), "%s handshake from %s:%d", ih_hex, remote_ip_.c_str(), remote_port_);
  return msg;
}

BBTSTorrentPeerPlugin::BBTSTorrentPeerPlugin(peer_connection& pc, libtorrent::torrent& t)
  : start_time_(time(NULL)),
    peer_connection_(pc),
    infohash_(t.info_hash()),
    session_(t.session()) {}

BBTSTorrentPeerPlugin::~BBTSTorrentPeerPlugin() {
  error_code ec;
  if (session_.m_alerts.should_post<peer_stat_alert>()) {
    session_.m_alerts.post_alert(
        peer_stat_alert(infohash_,
                        peer_connection_.remote().address().to_string(ec),
                        start_time_,
                        time(NULL),
                        peer_connection_.statistics().total_payload_upload(),
                        peer_connection_.statistics().total_payload_download(),
                        false));
  }
}

bool BBTSTorrentPeerPlugin::on_handshake(char const* reserved_bits) {
  error_code ec;
  if (session_.m_alerts.should_post<peer_handshake_alert>()) {
    const tcp::endpoint &remote = peer_connection_.remote();
    session_.m_alerts.post_alert(
        peer_handshake_alert(infohash_, remote.address().to_string(ec), remote.port()));
  }
  return true;
}

} // namespace bbts
