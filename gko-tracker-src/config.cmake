find_path(BOOST_H NAMES boost/system/error_code.hpp)
find_path(TR1_H NAMES tr1/functional)
find_library(BOOST_SYSTEM_LIB NAMES boost_system)
find_library(BOOST_THREAD_LIB NAMES boost_thread)
if(NOT BOOST_H OR NOT TR1_H OR NOT BOOST_SYSTEM_LIB OR NOT BOOST_THREAD_LIB)
    message(FATAL_ERROR "boost library(boost_system/boost_thread) not found.")
endif()

find_path(PROTOBUF_H NAMES google/protobuf/message.h)
find_library(PROTOBUF_LIB NAMES protobuf)
if(NOT PROTOBUF_H OR NOT PROTOBUF_LIB)
    message(FATAL_ERROR "protobuf not found.")
endif()

find_path(LIBEVENT_H NAMES event.h)
if (BUILD_BAIDU)
    find_library(LIBEVENT_LIB NAMES libevent.a)
else()
    find_library(LIBEVENT_LIB NAMES event)
endif()
if(NOT LIBEVENT_H OR NOT LIBEVENT_LIB)
    message(FATAL_ERROR "libevent not found.")
endif()

find_path(THRIFT_H NAMES thrift/Thrift.h)
find_library(THRIFT_LIB NAMES thrift)
find_library(THRIFT_NB_LIB NAMES thriftnb)
if(NOT THRIFT_H OR NOT THRIFT_LIB OR NOT THRIFT_NB_LIB)
    message(FATAL_ERROR "thrift not found.")
endif()

find_path(GFLAGS_H NAMES gflags/gflags.h)
find_library(GFLAGS_LIB NAMES gflags)
if(NOT GFLAGS_H OR NOT GFLAGS_LIB)
    message(FATAL_ERROR "gflags not found.")
endif()

find_path(GLOG_H NAMES glog/logging.h)
find_library(GLOG_LIB NAMES glog)
if(NOT GLOG_H OR NOT GLOG_LIB)
    message(FATAL_ERROR "glog not found.")
endif()

find_path(HIREDIS_H NAMES hiredis.h)
find_library(HIREDIS_LIB NAMES hiredis)
if(NOT HIREDIS_H OR NOT HIREDIS_LIB)
    message(FATAL_ERROR "hiredis not found.")
endif()

include_directories(${PROJECT_SOURCE_DIR}/include
                    ${PROJECT_SOURCE_DIR}/protocol
                    ${PROJECT_SOURCE_DIR}/minihttpd/include
                    ${TR1_H}
                    ${BOOST_H}
                    ${PROTOBUF_H}
                    ${LIBEVENT_H}
                    ${THRIFT_H}
                    ${GLOG_H}
                    ${GFLAGS_H}
                    ${HIREDIS_H})

if("$ENV{SCMPF_MODULE_VERSION}" STREQUAL "")
    set(BBTS_TRACKER_VERSION "\"1.0.0\"")
else()
    set(BBTS_TRACKER_VERSION "\"$ENV{SCMPF_MODULE_VERSION}\"")
endif()

add_definitions(-DHAVE_NETINET_IN_H
                -DHAVE_NETDB_H=1
                -DBBTS_TRACKER_VERSION=${BBTS_TRACKER_VERSION})
if(BUILD_BAIDU)
    add_definitions(-DBUILD_BAIDU)
endif()
set(CMAKE_CXX_FLAGS "-g -fPIC -Wall -pipe -W -fpermissive -Wno-unused-function -Wno-unused-parameter -Wno-invalid-offsetof -Winline -Wpointer-arith -Wwrite-strings -Woverloaded-virtual -ftemplate-depth-128 -Wreorder -Wswitch -Wformat")

SET(default_file_permissions OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)
SET(default_excute_permissions OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
