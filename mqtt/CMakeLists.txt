# driver

# we don't need to build documentation for mosquitto
option(DOCUMENTATION "Build documentation?" OFF)

option(WITH_STATIC_LIBRARIES "Build static versions of the libmosquitto/pp libraries?" OFF)
option(WITH_PIC "Build the static library with PIC (Position Independent Code) enabled archives?" OFF)

if( STATIC_BUILD )
	set(WITH_STATIC_LIBRARIES ON)
	set(WITH_PIC ON)
	set(OPENSSL_USE_STATIC_LIBS ON)
endif()

set(LDFLAGS_EX "")
if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  set(LDFLAGS_EX ${LDFLAGS_EX} "-undefined dynamic_lookup")
endif()

add_library(driver SHARED driver.c)

if( DEFINED STATIC_BUILD )
	set(CMAKE_C_FLAGS "-ldl -lpthread")
	add_subdirectory(../third_party/mosquitto ../third_party/mosquitto/build EXCLUDE_FROM_ALL)
	include_directories(../third_party/mosquitto/lib)
	if( STATIC_BUILD )
		target_link_libraries(driver libmosquitto_static ${LDFLAGS_EX})
	else()
		target_link_libraries(driver libmosquitto ${LDFLAGS_EX} -rdynamic)
	endif()
else()
	target_link_libraries(driver ${MOSQUITTO_LIBRARIES} ${LDFLAGS_EX})
endif()
set_target_properties(driver PROPERTIES PREFIX "" OUTPUT_NAME driver)

install(TARGETS driver DESTINATION ${TARANTOOL_INSTALL_LIBDIR}/mqtt)
install(FILES init.lua DESTINATION ${TARANTOOL_INSTALL_LUADIR}/mqtt)
