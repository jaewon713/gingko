SET(BBTS_PROTOCOL_SRC tracker_constants.cpp
                      tracker_types.cpp
                      announce_constants.cpp
                      announce_types.cpp
                      Announce.cpp
                      inner_constants.cpp
                      inner_types.cpp)

SET(BBTS_PROTOCOL_HEADER tracker_constants.h
                         tracker_types.h
                         announce_constants.h
                         announce_types.h
                         Announce.h
                         inner_constants.h
                         inner_types.h)

ADD_LIBRARY(bbts_protocol STATIC ${BBTS_PROTOCOL_SRC})
INCLUDE(${PROJECT_SOURCE_DIR}/config.cmake)

INSTALL(TARGETS bbts_protocol ARCHIVE DESTINATION lib)
INSTALL(FILES ${BBTS_PROTOCOL_HEADER} DESTINATION include/bbts/tracker)