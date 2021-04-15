cmake_minimum_required(VERSION 3.10)
project(jetson_watcher C)

set(CMAKE_C_STANDARD 99)

find_package(PkgConfig REQUIRED)

pkg_search_module(GLIB REQUIRED glib-2.0)
pkg_search_module(GSTREAMER REQUIRED gstreamer-1.0)

include_directories(
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
)

# ------------------------------------------
# WATCHER
add_executable(jetson_watcher src/watcher.c
        src/scene.c)

target_link_libraries(jetson_watcher PUBLIC
        ${GLIB_LIBRARIES}
        ${GSTREAMER_LIBRARIES})
# ------------------------------------------