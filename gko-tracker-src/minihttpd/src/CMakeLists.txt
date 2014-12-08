SET(MINIHTTPD_SRC
    minihttpd
    eventloop
    current_thread
    timestamp
    process_info
    process_inspector
    monitor_helper)

SET(MINIHTTPD_HEADER
    closure.h
    common.h
    current_thread.h
    eventloop.h
    minihttpd.h
    monitor_helper.h
    mutex.h
    path.h
    process_info.h
    process_inspector.h
    scoped_ptr.h
    spinlock.h
    strutil.h
    timestamp.h
    types.h)

ADD_LIBRARY(minihttpd STATIC ${MINIHTTPD_SRC})
INCLUDE(${PROJECT_SOURCE_DIR}/config.cmake)
