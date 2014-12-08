CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(bbts_tracker)
ADD_SUBDIRECTORY(minihttpd)
ADD_SUBDIRECTORY(protocol)
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(clientmock)

SET(default_file_permissions OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)
SET(default_excute_permissions OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
INSTALL(DIRECTORY conf/ DESTINATION conf
        FILE_PERMISSIONS ${default_file_permissions}
        DIRECTORY_PERMISSIONS ${default_excute_permissions}
        PATTERN .svn EXCLUDE)
INSTALL(DIRECTORY bin/ DESTINATION bin
        FILE_PERMISSIONS ${default_excute_permissions}
        DIRECTORY_PERMISSIONS ${default_excute_permissions}
        PATTERN .svn EXCLUDE)
if(BUILD_BAIDU)
    INSTALL(DIRECTORY opbin/ DESTINATION opbin
            FILE_PERMISSIONS ${default_excute_permissions}
            DIRECTORY_PERMISSIONS ${default_excute_permissions}
            PATTERN .svn EXCLUDE)
    INSTALL(DIRECTORY noahdes/ DESTINATION noahdes
            FILE_PERMISSIONS ${default_file_permissions}
            DIRECTORY_PERMISSIONS ${default_excute_permissions}
            PATTERN .svn EXCLUDE)
endif()