cmake_minimum_required(VERSION 3.5)
project(socket-tests)

include(ExternalGTest.cmake)

find_package(Threads REQUIRED)
add_compile_options(-std=c++17 -pedantic -Wall)

file(GLOB TESTS tests/*.cpp)
add_executable(${PROJECT_NAME} test-runner.cpp ${TESTS})
target_link_libraries(${PROJECT_NAME} ${GTEST_LIBRARY} rt socket)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${PROJECT_NAME})

