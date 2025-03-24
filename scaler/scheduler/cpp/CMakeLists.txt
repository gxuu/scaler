
cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

project(
  scheduler_cpp
  LANGUAGES C CXX
  VERSION 0.1
)

add_executable(scheduler_cpp)


if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
  message(STATUS "scheduler_cpp Release build")
else()
  set(CMAKE_BUILD_TYPE "Debug")
  message(STATUS "scheduler_cpp Debug build")
endif()

#find cppzmq wrapper, installed by make of cppzmq
find_package(cppzmq REQUIRED)
message(STATUS "found cppzmq")

target_link_libraries(scheduler_cpp PUBLIC cppzmq)
# Or use static library to link
# target_link_libraries(scheduler_cpp cppzmq-static)


target_sources(scheduler_cpp PRIVATE
  # address_monitor.h
  # async_binder.h
  # async_connector.h
  # client_manager.h
  # context.h
  # graphtask_manager.h
  # scheduler.h
  # status_reporter.h
  worker_manager.cpp
  task_manager.cpp
  object_manager.cpp
  main.cpp
)


find_package(CapnProto CONFIG REQUIRED)

capnp_generate_cpp(messageSources messageHeaders message.capnp status.capnp common.capnp)

target_sources(scheduler_cpp PRIVATE ${messageSources})
target_link_libraries(scheduler_cpp PRIVATE CapnProto::capnp)
target_include_directories(scheduler_cpp PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
