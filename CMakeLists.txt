cmake_minimum_required(VERSION 3.0)
project(ReadBFSWindows)

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fpermissive")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")

aux_source_directory(src SRC)
aux_source_directory(src/bfs SRC)
include_directories(src)
include_directories(src/bfs)
add_executable(ReadBFSWindows WIN32 ${SRC} src/resource/ReadBFSWindows.rc)
target_link_libraries(ReadBFSWindows ntdll comctl32)
