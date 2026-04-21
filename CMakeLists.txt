cmake_minimum_required(VERSION 3.10)
project(MiniDB)

set(CMAKE_CXX_STANDARD 17)

include_directories(src)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(mini_db ${SOURCES})