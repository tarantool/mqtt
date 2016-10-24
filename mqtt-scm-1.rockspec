package = "mqtt"
version = "scm-1"
source = {
	url = "git://github.com/tarantool/mqtt.git",
	tag = "master"
}
description = {
	summary = "Mqtt connector for Tarantool",
	homepage = "https://github.com/tarantool/mqtt",
	license = "MIT"
}
dependencies = {
	"lua >= 5.1"
}
external_dependencies = {
        TARANTOOL = {
                header = "tarantool/module.h"
        },
	LIBMOSQUITTO = {
		header = "mosquitto.h"
	}
}
build = {
	type = "cmake"
}
