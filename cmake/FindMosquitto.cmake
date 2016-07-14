# - Find libmosquitto
# Find the native libmosquitto includes and libraries
#
#  MOSQUITTO_INCLUDE_DIR - where to find mosquitto.h, etc.
#  MOSQUITTO_LIBRARIES   - List of libraries when using libmosquitto.
#  MOSQUITTO_FOUND       - True if libmosquitto found.

if(MOSQUITTO_INCLUDE_DIR)
  set(MOSQUITTO_FIND_QUIETLY TRUE)
endif()

find_path(MOSQUITTO_INCLUDE_DIR mosquitto.h)

find_library(MOSQUITTO_LIBRARY NAMES libmosquitto)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
  MOSQUITTO DEFAULT_MSG
  MOSQUITTO_LIBRARY MOSQUITTO_INCLUDE_DIR)

if(MOSQUITTO_FOUND)
  set(MOSQUITTO_LIBRARIES ${MOSQUITTO_LIBRARY})
else(MOSQUITTO_FOUND)
  set(MOSQUITTO_LIBRARIES)
endif()

mark_as_advanced(MOSQUITTO_INCLUDE_DIR MOSQUITTO_LIBRARY)
