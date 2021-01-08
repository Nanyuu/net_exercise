cmake_minimum_required(VERSION 3.17)
project(chat_room)

set(CMAKE_CXX_STANDARD 14)

find_package(fmt)

add_executable(chat_room chat_room/client.cpp chat_room/server.cpp)

target_link_libraries(chat_room PRIVATE fmt::fmt)