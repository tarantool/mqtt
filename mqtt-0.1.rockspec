package = "mqtt"
version = "0.1"
source = {
	url = "git://github.com/tarantool/mqtt",
	tag = "v0.1"
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
  TARANTOO = "tarantool/module.h" {
  }
	LIBMOSQUITTO = {
		header = "mosquitto.h"
	}
}
build = {
	type = "cmake",
}
