cmake_minimum_required(VERSION 3.10)
project(MiniDB)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src)

file(GLOB_RECURSE SOURCES "src/*.cpp")

# Executable name (CLI command)
add_executable(minidb ${SOURCES})

# ---- Always copy final binary to dist/ ----
add_custom_command(TARGET minidb POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/dist
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:minidb>
            ${CMAKE_SOURCE_DIR}/dist/
)