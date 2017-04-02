# driver

add_library(driver SHARED driver.c)

set(LDFLAGS_EX "")
if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  set(LDFLAGS_EX ${LDFLAGS_EX} "-undefined dynamic_lookup")
endif()

message(STATUS ${MOSQUITTO_LIBRARIES})

target_link_libraries(driver ${MOSQUITTO_LIBRARIES} ${LDFLAGS_EX} -rdynamic)
set_target_properties(driver PROPERTIES PREFIX "" OUTPUT_NAME "driver")

install(TARGETS driver LIBRARY DESTINATION ${TARANTOOL_INSTALL_LIBDIR}/mqtt)
install(FILES init.lua DESTINATION ${TARANTOOL_INSTALL_LUADIR}/mqtt)
