SET(GINGKO_SRC
    log
    encode
    Task
    TaskManager
    util
    WorkerPool
    host_info_util
    ErrorCategory
    socket_util
    message.pb
    TrackerConf.pb
    configure.pb
    path
    bbts_agent
    ThriftTracker
    TaskDB
    number_util
    Syms
    make_torrent
    dump_torrent
    string_util
    BBTSClient
    UnixSocketConnection
    UnixSocketServer
    UnixSocketClient
    delete_files
    BBTSTorrentPlugin
    subnet_mask
    Downloader
    ClusterDownloader
    SpeedLimit
    gko3)

include(${PROJECT_SOURCE_DIR}/config.cmake)
add_executable(gko3 ${GINGKO_SRC})
target_link_libraries(gko3 sqlite3
                           gingko_group
                           ${BBTS_PROTOCOL_LIB}
                           ${ULLIB_LIB}
                           ${LOG4CPP_LIB}
                           ${LIBTORRENT_LIB}
                           ${BOOST_SYSTEM_LIB}
                           ${BOOST_THREAD_LIB}
                           ${BOOST_REGEX_LIB}
                           ${THRIFT_LIB}
                           ${PROTOBUF_LIB}
                           pthread dl rt)
                           
add_executable(file_is_locked file_is_locked)

install(TARGETS gko3 file_is_locked RUNTIME DESTINATION bin)
