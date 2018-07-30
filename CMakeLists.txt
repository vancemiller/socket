cmake_minimum_required(VERSION 3.5)
project(socket)

option(SOCKET_TESTS "Run unit tests" OFF)
if(SOCKET_TESTS)
  add_subdirectory(test)
endif()

if(NOT TARGET lock)
  add_subdirectory(lock)
endif()

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)
target_link_libraries(${PROJECT_NAME} INTERFACE lock)

