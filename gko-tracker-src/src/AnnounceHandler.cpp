#include "bbts/tracker/AnnounceHandler.h"

#include <glog/logging.h>

#include "bbts/StatusManager.h"
#include "bbts/tracker/TrackerErrorCategory.h"

using std::endl;
using std::string;
using boost::system::error_code;

namespace bbts {
namespace tracker {

int AnnounceHandler::MIN_INTERVAL = 2;

// thrift request interface
void AnnounceHandler::announce(AnnounceResponse &_return, const AnnounceRequest &_request) {
  g_pStatusManager->IncreaseItemCounting("request");
  if (_request.__isset.infohash == false || _request.__isset.peer
      == false || _request.peer.__isset.ip == false) {
    AnnounceError(_return, "unknown", "No infohash or source ip");
    return;
  }

  std::stringstream request_tag;
  string base64_infohash;
  if (!Base64Encode(_request.infohash, &base64_infohash)) {
    base64_infohash = _request.infohash;
  }
  request_tag << "infohash:" << base64_infohash.c_str() << ", ip:" << _request.peer.ip.c_str();
  PeerInfo peer_info;
  LOG(INFO) << "new AnnounceRequest received, infohash:" << request_tag.str();
  if (true != PreHandleRequest(_request, &peer_info)) {
    AnnounceError(_return, request_tag.str(), "parse request error");
    return;
  }
  PeerList peer_list;
  bool have_seed = false;
  g_pStatusManager->IncreaseItemCounting("valid_request");
  int ret = peer_handler_.Handle(peer_info, &peer_list, have_seed);
  if (ret != 0) {
    AnnounceInfo(_return, ret, request_tag.str(), "handle request error");
    return;
  }
  if (true != ComposeResponse(peer_list, have_seed, &_return)) {
    AnnounceError(_return, request_tag.str(), "compose response error");
    return;
  }
  AnnounceSuccess(_return, request_tag.str());
}

void AnnounceHandler::AnnounceSuccess(AnnounceResponse &_return, const string &request_tag) {
  AnnounceInfo(_return, 0, request_tag, "successfully");
}

void AnnounceHandler::AnnounceError(AnnounceResponse &_return,
    const string &request_tag, const string &error_message) {
  AnnounceInfo(_return, -1, request_tag, error_message);
}

void AnnounceHandler::AnnounceInfo(AnnounceResponse &_return,
    int error_code, const string &request_tag, const string &message) {
  _return.__set_ret(error_code);
  _return.__set_failure_reason(message.c_str());
  _return.__set_min_interval(MIN_INTERVAL);
  string final_result = error_code == 0 ? "" : "failed";
  LOG(INFO) << "announce " << request_tag.c_str() << ", final:" << ":" << message.c_str();
}

/*
 * convert the basic info of input to the inner data structor
 */
bool AnnounceHandler::PreHandleRequest(const AnnounceRequest &request, PeerInfo *peer_info) {
  assert(peer_info);
  peer_info->SetTrackerId(PeerInfo::GetLocalTrackerId());
  std::stringstream peer_info_stream;
  peer_info_stream << "==========one peer message info start=============" << endl;
  if (request.__isset.infohash == true) {
    if (peer_info->Initialized() == false) {
      peer_info->SetInfoHash(request.infohash);
    }
    peer_info_stream << "infohash: " << peer_info->GetBase64InfoHash() << endl;
  } else {
    LOG(WARNING)<<"input: no infohash";
    return false;
  }
  if (request.__isset.peer == true) {
    if (peer_info->Initialized() == false) {
      peer_info->SetPeerId(request.peer.peerid);
    }
    peer_info_stream << "peerid: " << peer_info->GetBase64PeerId() << endl;
    peer_info->SetIp(request.peer.ip);
    peer_info_stream << "ip: " << peer_info->GetIp() << endl;
    string idc;
    if (request.peer.__isset.idc == false) {
      idc = "default";
    } else {
      idc = request.peer.idc;
    }
    peer_info->SetIdc(idc);
    peer_info_stream << "idc: "<<peer_info->GetIdc() << endl;
    peer_info->SetPort(request.peer.port);
    peer_info_stream << "port: " << peer_info->GetPort() << endl;
  } else {
    LOG(WARNING)<<"input: no peer";
    return false;
  }
  if (request.__isset.stat == true) {
    peer_info->SetUploaded(request.stat.uploaded);
    peer_info_stream << "uploaded: " << peer_info->GetUploaded() << endl;
    peer_info->SetDownloaded(request.stat.downloaded);
    peer_info_stream << "downloaded: " << peer_info->GetDownloaded() << endl;
    peer_info->SetLeft(request.stat.left);
    peer_info_stream << "left: " << peer_info->GetLeft() << endl;
    peer_info->SetStatus(request.stat.status);
    peer_info_stream << "status: " << (int)(peer_info->GetStatus()) << endl;
  } else {
    LOG(WARNING) << "input: no stat";
    return false;
  }
  if (request.__isset.is_seed == true) {
    peer_info->SetIsSeed(request.is_seed);
    peer_info_stream << "is seed: "<< peer_info->GetIsSeed() << endl;
  } else {
    LOG(WARNING)<<"input: no is_seed";
    return false;
  }
  if (request.__isset.num_want == true) {
    peer_info->SetWantNumber(request.num_want);
    peer_info_stream << "want num: " << peer_info->GetWantNumber() << endl;
  } else {
    LOG(WARNING)<<"input: no numwant";
    return false;
  }
  peer_info_stream << "==========one peer message info end=============" << endl;
  LOG(INFO) << peer_info_stream.str();
  return true;
}

/*
 * used to convert monitor_item_map to the thrift type
 */
bool AnnounceHandler::ComposeResponse(const PeerList &peer_list, bool have_seed,
                                     AnnounceResponse *response) {
  assert(response);

  PeerList::const_iterator item_iter = peer_list.begin();
  bool ret = true;

  for (; item_iter != peer_list.end(); item_iter++) {
    const PeerInfo &peer_info = static_cast<const PeerInfo &>(*item_iter);
    Peer response_peer;
    response_peer.__set_peerid(peer_info.GetPeerId());
    response_peer.__set_ip(peer_info.GetIp());
    response_peer.__set_port(peer_info.GetPort());
    response->peers.push_back(response_peer);
  }
  response->__set_have_seed(have_seed);
  LOG(INFO)<<"[FINAL]peers num:"<<response->peers.size()<<" with "<< (have_seed ? "seed" : "no seed");
  return ret;
}

static void FillBaseResponse(BaseResponse *_return, const error_code &ec, const string &tag) {
  LOG(INFO) << "StopByInfohash retval " << ec.value() << ": " << ec.message() << ", " << tag;
  _return->__set_retval(ec.value());
  _return->__set_message(ec.message());
}

void AnnounceHandler::ControlByInfohash(BaseResponse& _return,
                                        const ControlByInfohashRequest& request,
                                        int type) {
  error_code ec;
  if (!request.__isset.ip || !request.__isset.token || !request.__isset.infohash) {
    ec.assign(errors::MISSING_ARGS, get_error_category());
    FillBaseResponse(&_return, ec, "ip, token or infohash not set");
    return;
  }
  if (request.token != "bbts-tracker-control") {
    ec.assign(errors::INVALID_ARGS, get_error_category());
    FillBaseResponse(&_return, ec, "invalid token or infohash");
    return;
  }

  string base64_infohash;
  if (!Base64Encode(request.infohash, &base64_infohash)) {
    base64_infohash = request.infohash;
  }
  string tag = "infohash: " + base64_infohash + ", ip: " + request.ip;
  LOG(INFO) << "new ControlByInfohash(" << type << ") received, " << tag;
  peer_handler_.ControlByInfohash(base64_infohash, type, ec);
  FillBaseResponse(&_return, ec, "");
}

void AnnounceHandler::StopByInfohash(BaseResponse& _return, const ControlByInfohashRequest& request){
  ControlByInfohash(_return, request, PeerHandler::STOP_DOWNLOADING);
}

void AnnounceHandler::ResumeByInfohash(BaseResponse& _return, const ControlByInfohashRequest& request) {
  ControlByInfohash(_return, request, PeerHandler::RESUME_DOWNLOAD);
}

} // namespace tracker
} // namespace bbts

