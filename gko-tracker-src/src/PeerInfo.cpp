#include "bbts/tracker/PeerInfo.h"

#include "bbts/thrift_serializer.h"
#include "tracker_types.h"

using std::string;

namespace bbts {
namespace tracker {

string PeerInfo::tracker_id_;

PeerInfo::PeerInfo() : is_set_remote(false) {};

PeerInfo::PeerInfo(const PeerInfo &to_copy) {
  this->is_set_remote = to_copy.GetIsSetRemote();
  this->thrift_peer = to_copy.GetInnerPeer();
}

const tracker::StoredPeerInfo &PeerInfo::GetInnerPeer() const {
  return thrift_peer;
}

StoredPeerInfo &PeerInfo::GetInnerPeerReference() {
  return thrift_peer;
}

void PeerInfo::UpdateInfo(const PeerInfo& input_peer) {
  const StoredPeerInfo &input_thrift_peer = input_peer.GetInnerPeer();
  if (input_thrift_peer.__isset.info_hash == true) {
    this->SetInfoHash(input_thrift_peer.info_hash);
  } else {
    return;
  }

  if (input_thrift_peer.__isset.peer_id == true) {
    this->SetPeerId(input_thrift_peer.peer_id);
  } else {
    return;
  }

  uint64_t expire_interval;
  if (input_thrift_peer.__isset.expire_time == true) {
    expire_interval = input_thrift_peer.expire_time;
    this->SetExpireTime(expire_interval);
  }
  this->SetTimeStamp((int64_t)time(NULL));
  this->SetIp(input_thrift_peer.ip);
  this->SetPort(input_thrift_peer.port);
  this->SetStatus(input_thrift_peer.status);
  this->SetUploaded(input_thrift_peer.uploaded);
  this->SetDownloaded(input_thrift_peer.downloaded);
  this->SetLeft(input_thrift_peer.left);
  this->SetWantNumber(input_thrift_peer.want_number);
  this->SetIsSeed(input_thrift_peer.is_seed);
  this->SetTrackerId(input_thrift_peer.tracker_id);
  this->SetIdc(input_thrift_peer.idc);
}

void PeerInfo::ConstructPeerInfoByString(const string &string_input) {
  ThriftParseFromString(string_input, &(this->GetInnerPeerReference()));
  if (!Base64Encode(this->GetInfoHash(), &base64_info_hash)) {
    base64_info_hash = this->GetInfoHash();
  }
  if (Base64Encode(this->GetPeerId(), &base64_peer_id)) {
    base64_peer_id = this->GetPeerId();
  }
}

void PeerInfo::ConstructStringByPeerInfo(string *string_input) const {
  assert(string_input);
  ThriftSerializeToString(this->GetInnerPeer(), string_input);
}

}
}
