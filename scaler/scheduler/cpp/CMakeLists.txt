
cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# scaler.scheduler, scaler.utility, scaler.ui, scaler.object_usage, scaler.io, scaler.protocol
target_sources(scheduler_cpp 
  PRIVATE
  FILE_SET cxx_modules 
  TYPE CXX_MODULES 
  FILES
  scheduler.cppm
  task_manager.cppm
  worker_manager.cppm
  client_manager.cppm
  object_manager.cppm
  graphtask_manager.cppm
  status_reporter.cppm
)


target_sources(scheduler_cpp PRIVATE
  task_manager.cpp
  worker_manager.cpp
  object_manager.cpp
  # client_manager.cpp
  main.cpp
)



find_package(CapnProto CONFIG REQUIRED)

capnp_generate_cpp(messageSources messageHeaders message.capnp status.capnp common.capnp)

target_sources(scheduler_cpp PRIVATE ${messageSources})
target_link_libraries(scheduler_cpp PRIVATE CapnProto::capnp)
target_include_directories(scheduler_cpp PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
