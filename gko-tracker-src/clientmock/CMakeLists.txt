INCLUDE(${PROJECT_SOURCE_DIR}/config.cmake)
SET(MOCK_LINK_LIBS bbts_protocol ${THRIFTNB_LIB} ${THRIFT_LIB} ${BOOST_THREAD_LIB} ${BOOST_SYSTEM_LIB} pthread rt)

ADD_EXECUTABLE(client_mock_infohash client_mock_infohash.cpp)
TARGET_LINK_LIBRARIES(client_mock_infohash ${MOCK_LINK_LIBS})

ADD_EXECUTABLE(client_mock client_mock.cpp)
TARGET_LINK_LIBRARIES(client_mock ${MOCK_LINK_LIBS})

ADD_EXECUTABLE(client_mock_s client_mock_s.cpp)
TARGET_LINK_LIBRARIES(client_mock_s ${MOCK_LINK_LIBS})

ADD_EXECUTABLE(ControlByInfohash ControlByInfohash.cpp)
TARGET_LINK_LIBRARIES(ControlByInfohash ${MOCK_LINK_LIBS})

INSTALL(TARGETS client_mock_infohash client_mock client_mock_s ControlByInfohash RUNTIME DESTINATION clientmock)