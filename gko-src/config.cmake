find_path(BBTS_PROTOCOL_H NAMES bbts/tracker/Announce.h)
find_library(BBTS_PROTOCOL_LIB NAMES bbts_protocol)
if(NOT BBTS_PROTOCOL_H OR NOT BBTS_PROTOCOL_LIB)
    message(FATAL_ERROR "bbts_protocol not found.")
endif()

find_path(CURL_H NAMES curl/curl.h)
find_library(CURL_LIB NAMES curl)
if(NOT CURL_H OR NOT CURL_LIB)
    message(FATAL_ERROR "libcurl not found.")
endif()

find_path(BOOST_H NAMES boost/system/error_code.hpp)
find_path(TR1_H NAMES tr1/functional)
find_library(BOOST_SYSTEM_LIB NAMES boost_system)
find_library(BOOST_THREAD_LIB NAMES boost_thread)
find_library(BOOST_REGEX_LIB NAMES boost_regex)
if(NOT BOOST_H OR NOT_TR1_H OR NOT BOOST_SYSTEM_LIB OR NOT BOOST_THREAD_LIB OR NOT BOOST_REGEX_LIB)
    message(FATAL_ERROR "boost library(boost_system/boost_thread/boost_regex) not found.")
endif()

find_path(PROTOBUF_H NAMES google/protobuf/message.h)
find_library(PROTOBUF_LIB NAMES protobuf)
if(NOT PROTOBUF_H OR NOT PROTOBUF_LIB)
    message(FATAL_ERROR "protobuf not found.")
endif()

find_path(LIBEVENT_H NAMES event.h)
find_library(LIBEVENT_LIB NAMES event)
if(NOT LIBEVENT_H OR NOT LIBEVENT_LIB)
    message(FATAL_ERROR "libevent not found.")
endif()

find_path(THRIFT_H NAMES thrift/Thrift.h)
find_library(THRIFT_LIB NAMES thrift)
find_library(THRIFT_NB_LIB NAMES thriftnb)
if(NOT THRIFT_H OR NOT THRIFT_LIB OR NOT THRIFT_NB_LIB)
    message(FATAL_ERROR "thrift not found.")
endif()

find_path(LIBTORRENT_H NAMES libtorrent/torrent.hpp)
find_library(LIBTORRENT_LIB NAMES torrent-rasterbar)
if(NOT LIBTORRENT_H OR NOT LIBTORRENT_LIB)
    message(FATAL_ERROR "libtorrent not found.")
endif()

if (BUILD_BAIDU)
    find_path(ULLIB_H NAMES comlog/comlog.h)
    find_library(ULLIB_LIB NAMES ullib)
    if(NOT ULLIB_H OR NOT ULLIB_LIB)
        message(FATAL_ERROR "ullib not found.")
    endif()
    
    find_path(SSL_H NAMES openssl/ssl.h)
    find_library(SSL_LIB NAMES ssl)
    if(NOT SSL_H OR NOT SSL_LIB)
        message(FATAL_ERROR "libssl not found.")
    endif()

    find_path(CRYPTO_H NAMES openssl/crypto.h)
    find_library(CRYPTO_LIB NAMES crypto)
    if(NOT CRYPTO_H OR NOT CRYPTO_LIB)
        message(FATAL_ERROR "libcrypto not found.")
    endif()
else()
    find_path(LOG4CPP_H NAMES log4cpp/Category.hh)
    find_library(LOG4CPP_LIB NAMES log4cpp)
    if(NOT LOG4CPP_H OR NOT LOG4CPP_LIB)
        message(FATAL_ERROR "log for cpp not found")
    endif()
endif()

find_path(JSONCPP_H NAMES json/json.h)
find_library(JSONCPP_LIB NAMES jsoncpp)
if(NOT JSONCPP_H OR NOT JSONCPP_LIB)
    message(FATAL_ERROR "jsoncpp not found.")
endif()

find_path(HDFS_H NAMES hdfs.h)
find_library(HDFS_LIB NAMES hdfs)
find_library(JSIG_LIB NAMES jsig)
find_library(JVM_LIB NAMES jvm)
if (NOT HDFS_H OR NOT HDFS_LIB OR NOT JSIG_LIB OR NOT JVM_LIB)
    message(STATUS "not found hdfs or jvm library, ignore and not support download from hdfs!")
endif()

include_directories(${PROJECT_SOURCE_DIR}/include
                    ${BBTS_PROTOCOL_H}
                    ${TR1_H}
                    ${BOOST_H}
                    ${PROTOBUF_H}
                    ${LIBEVENT_H}
                    ${THRIFT_H}
                    ${LIBTORRENT_H}
                    ${ULLIB_H}
                    ${LOG4CPP_H}
                    ${JSONCPP_H}
                    ${CURL_H}
                    ${HDFS_H})

if("$ENV{SCMPF_MODULE_VERSION}" STREQUAL "")
    set(GINGKO_VERSION "\"1.0.0\"")
else()
    set(GINGKO_VERSION "\"$ENV{SCMPF_MODULE_VERSION}\"")
endif()

add_definitions(-D_XOPEN_SOURE=500
                -D_GNU_SOURCE
                -DHAVE_NETINET_IN_H
                -DHAVE_NETDB_H=1
                -DGINGKO_VERSION=${GINGKO_VERSION}
                -DTORRENT_NO_DEPRECATE=1
                -DTORRENT_DISABLE_ENCRYPTION=1
                -DTORRENT_DISABLE_GEO_IP=1
                -DTORRENT_DISABLE_DHT=1
                -DBOOST_ASIO_HASH_MAP_BUCKETS=1021
                -DBOOST_EXCEPTION_DISABLE=1
                -DBOOST_ASIO_ENABLE_CANCELIO=1
                -DBOOST_ASIO_DYN_LINK=1)
if(BUILD_BAIDU)
    add_definitions(-DBUILD_BAIDU)
endif()
set(CMAKE_CXX_FLAGS "-g -fPIC -Wall -pipe -W -fpermissive -Wno-unused-function -Wno-unused-parameter -Wno-invalid-offsetof -Winline -Wpointer-arith -Wwrite-strings -Woverloaded-virtual -ftemplate-depth-128 -Wreorder -Wswitch -Wformat")
