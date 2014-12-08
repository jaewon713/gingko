set(SQLITE3_HEADER sqlite3.h sqlite3ext.h)
include(${PROJECT_SOURCE_DIR}/config.cmake)

add_library(sqlite3 STATIC sqlite3)
add_executable(sqlite3-cmd sqlite3 shell)
target_link_libraries(sqlite3-cmd pthread dl)

#install(TARGETS sqlite3 ARCHIVE DESTINATION lib)
#install(TARGETS sqlite3-cmd RUNTIME DESTINATION bin)
#install(FILES ${SQLITE3_HEADER} DESTINATION include/sqlite3)