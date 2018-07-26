cmake_minimum_required(VERSION 3.5)
project(socket)

option(RUN_TESTS "Run unit tests" ON)

if(RUN_TESTS)
  add_subdirectory(test)
endif()

