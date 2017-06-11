project (ChitChat)
cmake_minimum_required (VERSION 2.8 FATAL_ERROR)

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable (monitor
                monitor.cpp)

add_executable (server
                server.cpp)

add_executable (client
                client.cpp)