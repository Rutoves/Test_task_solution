cmake_minimum_required(VERSION 3.12)
project(test_task)

#set(Boost_DEBUG 1)
find_package(Boost REQUIRED COMPONENTS  filesystem regex)
include_directories(${Boost_INCLUDE_DIRS})
add_executable(test_task main.cpp)
target_link_libraries(test_task)
target_link_libraries(test_task ${Boost_LIBRARIES})