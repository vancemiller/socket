cmake_minimum_required(VERSION 3.5)
project(socket)

option(SOCKET_TESTS "Run unit tests" OFF)

if(SOCKET_TESTS)
  add_subdirectory(test)
endif()

