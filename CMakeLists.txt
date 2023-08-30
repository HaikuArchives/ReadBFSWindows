cmake_minimum_required(VERSION 3.0)
project(ReadBFSWindows)

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fpermissive -Wno-multichar -Wno-write-strings -D_FILE_OFFSET_BITS=64")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -Wno-multichar -Wno-write-strings -D_FILE_OFFSET_BITS=64")

aux_source_directory(src SRC)
aux_source_directory(src/bfs SRC)
include_directories(src)
include_directories(src/bfs)
add_executable(ReadBFSWindows WIN32 ${SRC} src/resource/ReadBFSWindows.rc)
target_link_libraries(ReadBFSWindows ntdll comctl32)
