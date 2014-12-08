namespace cpp bbts.tracker
struct Peer {
1: optional string peerid;
2: optional string ip;
3: optional i32 port;
4: optional string idc;
}

enum Status {
    METADATA,
    DOWNLOAD,
    SEEDING,
    STOPPED,
    PAUSED,
}

struct Stat {
1: optional i64    uploaded;
2: optional i64    downloaded;
3: optional i64    left;
4: optional Status status;
}

struct AnnounceRequest {
1: optional string infohash;
2: optional Peer   peer;
3: optional Stat   stat;
4: optional bool   is_seed;
5: optional i32    num_want;
}

struct AnnounceResponse {
1: optional i16    ret;
2: optional string failure_reason;
3: optional i32    min_interval;
4: optional bool   have_seed;
5: list<Peer>      peers;
}

struct ControlByInfohashRequest {
1: optional string ip;
2: optional string token;
3: optional string infohash;
}

struct BaseResponse {
1: optional i16    retval;
2: optional string message;
}
