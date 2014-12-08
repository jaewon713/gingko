cmake_minimum_required(VERSION 2.6)
set(CMAKE_SUPPRESS_REGENERATION 1)
set(CMAKE_USE_RELATIVE_PATHS ON)
project(gingko)
include(config.cmake)
add_subdirectory(sqlite)
add_subdirectory(group)
add_subdirectory(src)
add_subdirectory(plugin)
if (BUILD_BAIDU)
    add_subdirectory(serve)
endif()

install(DIRECTORY conf NOAH
        DESTINATION .
        FILE_PERMISSIONS ${default_file_permissions}
        DIRECTORY_PERMISSIONS ${default_excute_permissions}
        PATTERN .svn EXCLUDE)
