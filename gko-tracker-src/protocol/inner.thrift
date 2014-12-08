include "tracker.thrift"

namespace cpp bbts.tracker

struct StoredPeerInfo {
1:  optional string info_hash;
2:  optional string peer_id;
3:  optional string ip;
4:  optional i32 port;
5:  optional i64 uploaded;
6:  optional i64 downloaded;
7:  optional i64 left;
8:  optional tracker.Status status;
9:  optional i32 want_number;
10: optional i64 expire_time;
11: optional i64 timestamp_start;
12: optional bool is_seed;
13: optional string tracker_id;
14: optional string idc;
}
