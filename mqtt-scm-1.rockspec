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
build = {
    type = "cmake";
    variables = {
        CMAKE_BUILD_TYPE="RelWithDebInfo";
        TARANTOOL_INSTALL_LIBDIR="$(LIBDIR)";
        TARANTOOL_INSTALL_LUADIR="$(LUADIR)";
    };
}
