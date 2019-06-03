package = "mqtt"
version = "1.4.0-2"
source = {
    url = "git://github.com/tarantool/mqtt.git",
    tag = "1.4.0"
}
description = {
    summary = "Mqtt connector for Tarantool",
    homepage = "https://github.com/tarantool/mqtt",
    license = "BSD"
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
        STATIC_BUILD="$(STATIC_BUILD)";
        OPENSSL_ROOT_DIR="/usr/local/opt/openssl";
    };
    platforms = {
        macosx = {
            variables = {
                OPENSSL_ROOT_DIR="/usr/local/opt/openssl";
            };
        };
    };
}
