cmake_minimum_required(VERSION 2.8)

project(mqtt C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Find deps
find_package(Tarantool REQUIRED)
include_directories(${TARANTOOL_INCLUDE_DIRS})

if (NOT DEFINED STATIC_BUILD)
	find_package(Mosquitto REQUIRED)
	include_directories(${MOSQUITTO_INCLUDE_DIRS})
	message(STATUS "libmosquitto: ${MOSQUITTO_INCLUDE_DIRS} ${MOSQUITTO_LIBRARIES}")
endif()

message(STATUS "tarantool: ${TARANTOOL_INCLUDE_DIRS}")

# Set CFLAGS
set(MY_C_FLAGS "-Wall -Wextra -Werror -std=gnu11 -fno-strict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MY_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${MY_C_FLAGS}")

include_directories(${TARANTOOL_INCLUDE_DIRS})

# Build module
add_subdirectory(mqtt)

add_custom_target(test
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/run_all.sh
                  DEPENDS driver)
