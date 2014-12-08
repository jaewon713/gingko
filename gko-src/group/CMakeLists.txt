set(GINGKO_GROUP_SRC
    bbts_group
    GroupConf.pb
    group_manager
    group_manager_service_handler
    group_client
    group_task
    utils
    group_cluster_task
    gen-cpp/GroupManagerService
    gen-cpp/GroupService_constants
    gen-cpp/GroupService_types)

include(${PROJECT_SOURCE_DIR}/config.cmake)
add_library(gingko_group STATIC ${GINGKO_GROUP_SRC})

install(PROGRAMS bbts-group-control supervise.bbts-group DESTINATION bin)
