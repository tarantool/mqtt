cmake_minimum_required(VERSION 2.8)

project(mqtt C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Find deps
find_package(Tarantool REQUIRED)
include_directories(${TARANTOOL_INCLUDE_DIRS})

find_package(Mosquitto REQUIRED)
include_directories(${MOSQUITTO_INCLUDE_DIRS})

# Set CFLAGS
set(MY_C_FLAGS "-Wall -Wextra -Werror -pedantic -std=gnu99")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MY_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${MY_C_FLAGS}")

# Build module
add_subdirectory(mqtt)
