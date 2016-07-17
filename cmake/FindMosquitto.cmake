# - Find libmosquitto
# Find the native libmosquitto includes and libraries
#
#  MOSQUITTO_INCLUDE_DIR - where to find mosquitto.h, etc.
#  MOSQUITTO_LIBRARIES   - List of libraries when using libmosquitto.
#  MOSQUITTO_FOUND       - True if libmosquitto found.

find_path(
  MOSQUITTO_INCLUDE_DIR mosquitto.h
  HINTS ENV MOSQ_INCLUDE_DIR)

find_library(
  MOSQUITTO_LIBRARY
  NAMES libmosquitto libmosquitto.so.1
  HINTS ${MOSQ_LIB_DIR} ENV MOSQ_LIB_DIR
  PATHS ${MOSQ_LIB_DIR} ENV MOSQ_LIB_DIR)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
  MOSQUITTO DEFAULT_MSG
  MOSQUITTO_LIBRARY MOSQUITTO_INCLUDE_DIR)

message(STATUS "libmosquitto include dir: ${MOSQUITTO_INCLUDE_DIR}")
message(STATUS "libmosquitto: ${MOSQUITTO_LIBRARY}")
set(MOSQUITTO_LIBRARIES ${MOSQUITTO_LIBRARY})

mark_as_advanced(MOSQUITTO_INCLUDE_DIR MOSQUITTO_LIBRARY)
