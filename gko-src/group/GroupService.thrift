namespace cpp bbts.group

struct GeneralResponse {
  1:i32 ret_code,
  2:string message,
}

struct UserCred {
  1:i32 pid,
  2:i32 gid,
  3:i32 uid,
}

struct GroupManagerOptions {
  1:optional i32 bind_port,
  2:optional i32 upload_limit,
  3:optional i32 download_limit,
  4:optional i32 max_connections,
  5:optional i64 timeout,
}

struct ListenPortRange {
  1:i32 start_port = 18000,  // need rand
  2:i32 end_port = 65534,
}

struct TrackersPair {
  1:string name,
  2:i32 id,
}

struct ClusterParam {
  1:string host,
  2:i32 port,
  3:string user,
  4:string passwd,
  5:string prefix_path,
}

struct DownloadParam {
  1:i32 download_limit = 10485760,  // 10M
  2:i32 upload_limit = 10485760,    // 10M
  3:i32 connections_limit = 8000,
  4:i32 seeding_time = -2,
  5:i32 timeout = 0,
  6:i32 hang_timeout = 0,
  7:i32 mem_limit = 0,
  8:bool debug = false,
  9:bool print_progress = false,
  10:bool patition_download = false,
  11:bool need_save_resume = false,
  12:bool need_down_to_tmp_first = false,
  13:bool quit_by_tracker_failed = false,
  14:bool storage_pre_allocate = false,
  15:i32 dynamic_allocate = -1,
  16:bool use_dio_read = false,
  17:bool use_dio_write = false,
  18:bool ignore_hdfs_error = false,
  19:string torrent_path,
  20:string infohash,
  21:string torrent_url,
  22:string new_name,
  23:string save_path,
  24:ClusterParam cluster_config,
  25:list<string> web_seeds,
  26:ListenPortRange listen_port_range,
  27:list<string> include_regex,
  28:list<string> exclude_regex,
  29:string control_path,
  30:string save_torrent_path,
  31:string cmd,
  32:string filter,
  33:list<TrackersPair> trackers,
  34:i32    cluster_thread_num = 3,
}

struct TaskStatus {
  1:string  hostname,
  2:string  ip,
  3:i32     port = 0,
  4:string  infohash,
  5:i64     payload_downloaded = 0,
  6:i64     payload_uploaded = 0,
  7:i32     progress_ppm = 0,
  8:i64     start_time = 0,
  9:i64     end_time = 0,
  10:i32    time_for_download_metadata = 0,
  11:i32    time_for_download = 0,
  12:i32    time_for_check_files = 0,
  13:i32    time_for_seeding = 0,
  14:i32    download_limit = 0,
  15:i32    upload_limit = 0,
  16:i32    num_files = 0,
  17:i32    num_paths = 0,
  18:i32    num_symlinks = 0,
  19:i32    piece_length = 0,
  20:i32    num_pieces = 0,
  21:bool   is_hdfs_download = false,
  22:string hdfs_address,
  23:i64    downloaded_from_hdfs = 0,
  24:i32    state = -1,
  25:double progress = 0,
  26:i32    download_rate = 0,
  27:i32    upload_rate = 0,
  28:bool   is_task_finished = false,
  29:i32    retval = 0,
  30:string state_string,
  31:i32    uid = -1,
  32:string save_path,
}

enum ControlTaskType {
  UNKNOWN = 0,
  CANCEL  = 1,
  PAUSE   = 2,
  RESUME  = 3, 
}

struct TaskOptions {
  1:optional string  infohash,
  2:optional i32     download_limit = -1,
  3:optional i32     upload_limit = -1,
  4:optional i32     connections_limit = -1,
}

service GroupManagerService {
  GeneralResponse AddGroupTask(1:DownloadParam down_params, 2:UserCred cred),
  list<TaskStatus> GetTaskStatus(1:string infohash, 2:bool is_full),
  GeneralResponse ControlTask(1:string infohash, 2:bool is_all, 3:ControlTaskType type, 4:UserCred cred),
  GeneralResponse SetTaskOption(1:TaskOptions options),
  list<TaskOptions> GetTaskOption(1:string infohash),
}

