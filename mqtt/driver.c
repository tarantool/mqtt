
/*
 * Copyright (C) 2016 - 2018 Tarantool AUTHORS: please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <mosquitto.h>

#include <tarantool/module.h>


#define TIMEOUT 1.0

#define MOSQ_LUA_UDATA_NAME "__tnt_mqtt_mosquitto"

typedef struct mosq_ctx {
  lua_State *L;
  struct mosquitto *mosq;
  int connect_ref;
  int disconnect_ref;
  int publish_ref;
  int message_ref;
  int subscribe_ref;
  int unsubscribe_ref;
  int log_ref;

  int log_level_mask;
} mosq_t;

enum callback_types {
    MESSAGE,
    CONNECT,
    PUBLISH,
    SUBSCRIBE,
    UNSUBSCRIBE,
    DISCONNECT,
};

enum connect_return_codes {
    CONN_ACCEPT,
    CONN_REF_BAD_PROTOCOL,
    CONN_REF_BAD_ID,
    CONN_REF_SERVER_NOAVAIL,
    CONN_REF_BAD_LOGIN,
    CONN_REF_NO_AUTH,
    CONN_REF_BAD_TLS
};

static bool mosq_initialized = false;

static inline mosq_t*
mosq_get(lua_State *L, int i)
{
    return (mosq_t *) luaL_checkudata(L, i, MOSQ_LUA_UDATA_NAME);
}

static inline int
make_str_result(lua_State *L, bool ok, const char *str)
{
    lua_pushboolean(L, ok);
    lua_pushstring(L, str);
    return 2;
}

static inline
int
make_int_result(lua_State *L, bool ok, int i)
{
    lua_pushboolean(L, ok);
    lua_pushinteger(L, i);
    return 2;
}

static inline
int
make_mosq_status_result(lua_State *L, int mosq_errno)
{
    switch (mosq_errno) {
    case MOSQ_ERR_SUCCESS:
      return make_str_result(L, true, "ok");

    case MOSQ_ERR_INVAL:
    case MOSQ_ERR_NOMEM:
    case MOSQ_ERR_PROTOCOL:
    case MOSQ_ERR_NOT_SUPPORTED:
    case MOSQ_ERR_NO_CONN:
    case MOSQ_ERR_CONN_LOST:
    case MOSQ_ERR_PAYLOAD_SIZE:
      return make_str_result(L, false, mosquitto_strerror(mosq_errno));
    case MOSQ_ERR_ERRNO:
      return make_str_result(L, false, strerror(errno));
    default:
      break;
    }
    return make_str_result(L, false, "unknown status");
}

static int
mosq_do_io_run_one(mosq_t *ctx, int max_packets)
{
    /** XXX
     * I have confused: socket < 0 means MOSQ_ERR_NO_CONN, or?
     */
    int rc = MOSQ_ERR_NO_CONN,
        revents = 0,
        fd = mosquitto_socket(ctx->mosq);

    if (fd >= 0) {

        rc = MOSQ_ERR_SUCCESS;

        if (mosquitto_want_write(ctx->mosq)) {
            revents = coio_wait(fd, COIO_WRITE, TIMEOUT);
            if (revents > 0) {
                rc = mosquitto_loop_write(ctx->mosq, max_packets);
            }
        } else {
            revents = coio_wait(fd, COIO_READ, TIMEOUT);
            if (revents > 0)
                rc = mosquitto_loop_read(ctx->mosq, max_packets);
        }
        /**
         * mosquitto_loop_miss
         * This function is handling PINGs and checking
         * whether messages need to be retried,
         * so should be called fairly _frequently_(!).
         * */
        if (rc == MOSQ_ERR_SUCCESS) {
            rc = mosquitto_loop_misc(ctx->mosq);
        }
    }

    return rc;
}


static
int
mosq_lib_version(lua_State *L)
{
    int major, minor, rev;
    char version[16];

    mosquitto_lib_version(&major, &minor, &rev);
    sprintf(version, "%i.%i.%i", major, minor, rev);

    return make_str_result(L, true, version);
}

static
int
mosq_lib_init(lua_State *L)
{
    if (!mosq_initialized)
        mosquitto_lib_init();

    return make_mosq_status_result(L, MOSQ_ERR_SUCCESS);
}

static
int
mosq_lib_destroy(lua_State *L)
{
    mosquitto_lib_cleanup();
    mosq_initialized = false;
    return make_mosq_status_result(L, MOSQ_ERR_SUCCESS);
}

static
int
mosq_new(lua_State *L)
{
    const char *id = luaL_optstring(L, 1, NULL);
    bool clean_session = (lua_isboolean(L, 2) ? lua_toboolean(L, 2) : true);

    if (id == NULL && !clean_session)
        return luaL_argerror(L, 2,
                "if 'id' is set then 'clean session' must be true");

    mosq_t *ctx = (mosq_t *) lua_newuserdata(L, sizeof(mosq_t));
    memset(ctx, 0, sizeof(mosq_t));

    ctx->L = L;
    ctx->connect_ref = LUA_REFNIL;
    ctx->disconnect_ref = LUA_REFNIL;
    ctx->publish_ref = LUA_REFNIL;
    ctx->message_ref = LUA_REFNIL;
    ctx->subscribe_ref = LUA_REFNIL;
    ctx->unsubscribe_ref = LUA_REFNIL;
    ctx->log_ref = LUA_REFNIL;

    ctx->mosq = mosquitto_new(id, clean_session, ctx);
    if (ctx->mosq == NULL)
        return luaL_error(L, strerror(errno));

    luaL_getmetatable(L, MOSQ_LUA_UDATA_NAME);
    lua_setmetatable(L, -2);

    return 1;
}

static int
mosq_destroy(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);

    if (ctx->mosq)
        mosquitto_destroy(ctx->mosq);

    ctx->mosq = NULL;

    luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->connect_ref);
    luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->disconnect_ref);
    luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->publish_ref);
    luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->message_ref);
    luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->subscribe_ref);
    luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->unsubscribe_ref);
    luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->log_ref);

    /* remove all methods operating on ctx */
    lua_newtable(L);
    lua_setmetatable(L, -2);

    return make_mosq_status_result(L, MOSQ_ERR_SUCCESS);
}

static int
mosq_will_set(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);
    const char *topic = luaL_checkstring(L, 2);
    size_t payloadlen = 0;
    const void *payload = NULL;

    if (!lua_isnil(L, 3))
        payload = lua_tolstring(L, 3, &payloadlen);

    const int qos = luaL_optinteger(L, 4, 0);
    const bool retain = lua_toboolean(L, 5);

    return make_mosq_status_result(L,
            mosquitto_will_set(
                ctx->mosq, topic, payloadlen, payload,
                qos, retain));
}

static
int
mosq_will_clear(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);
    return make_mosq_status_result(L, mosquitto_will_clear(ctx->mosq));
}

static
int
mosq_login_set(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);
    const char *username = (lua_isnil(L, 2) ? NULL : luaL_checkstring(L, 2));
    const char *password = (lua_isnil(L, 3) ? NULL : luaL_checkstring(L, 3));
    return make_mosq_status_result(L,
            mosquitto_username_pw_set(ctx->mosq, username, password));
}

static
int
mosq_tls_set(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);
    const char *cafile = luaL_optstring(L, 2, NULL);
    const char *capath = luaL_optstring(L, 3, NULL);
    const char *certfile = luaL_optstring(L, 4, NULL);
    const char *keyfile = luaL_optstring(L, 5, NULL);
    /*
        the last param is a callback to a function that asks for a passphrase
        for a keyfile our keyfiles should NOT have a passphrase
    */
    return make_mosq_status_result(L,
        mosquitto_tls_set(ctx->mosq, cafile, capath, certfile, keyfile, 0));
}

static
int
mosq_tls_insecure_set(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);
    const bool value = lua_toboolean(L, 2);
    return make_mosq_status_result(L,
            mosquitto_tls_insecure_set(ctx->mosq, value));
}

static
int
mosq_connect(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);
    const char *host = luaL_optstring(L, 2, "localhost");
    const int port = luaL_optinteger(L, 3, 1883);
    const int keepalive = luaL_optinteger(L, 4, 60);
    return make_mosq_status_result(L,
        mosquitto_connect(ctx->mosq, host, port, keepalive));
}

static
int
mosq_reconnect(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);

    int rc = mosquitto_reconnect(ctx->mosq);
    return make_mosq_status_result(L, rc);
}

static
int
mosq_disconnect(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);

    int rc = mosquitto_disconnect(ctx->mosq);
    return make_mosq_status_result(L, rc);
}

static
int
mosq_publish(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);
    /* message id is referenced in the publish callback */
    int mid = -1;
    const char *topic = luaL_checkstring(L, 2);
    size_t payloadlen = 0;
    const void *payload = NULL;

    if (!lua_isnil(L, 3)) {
        payload = lua_tolstring(L, 3, &payloadlen);
    };

    int qos = luaL_optinteger(L, 4, 0);
    bool retain = lua_toboolean(L, 5);

    int rc = mosquitto_publish(ctx->mosq, &mid, topic, payloadlen, payload,
            qos, retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        return make_mosq_status_result(L, rc);
    }

    return make_int_result(L, true, mid);
}

static
int
mosq_subscribe(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);
    int mid;
    const char *sub = luaL_checkstring(L, 2);
    int qos = luaL_optinteger(L, 3, 0);

    int rc = mosquitto_subscribe(ctx->mosq, &mid, sub, qos);

    if (rc != MOSQ_ERR_SUCCESS) {
        return make_mosq_status_result(L, rc);
    }

    return make_int_result(L, true, mid);
}

static
int
mosq_unsubscribe(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);
    int mid;
    const char *sub = luaL_checkstring(L, 2);

    int rc = mosquitto_unsubscribe(ctx->mosq, &mid, sub);

    if (rc != MOSQ_ERR_SUCCESS) {
        return make_mosq_status_result(L, rc);
    }

    return make_int_result(L, true, mid);
}

static void
mosq_connect_f(struct mosquitto *mosq __attribute__((unused)),
        void *obj, int rc)
{
    mosq_t *ctx = obj;
    if (!ctx || !ctx->mosq)
        return;
    enum connect_return_codes return_code = rc;

    bool success = false;
    const char *str = "connection status unknown";

    switch (return_code) {
    case CONN_ACCEPT:
        success = true;
        str = "connection accepted";
        break;

    case CONN_REF_BAD_PROTOCOL:
        str = "connection refused - incorrect protocol version";
        break;

    case CONN_REF_BAD_ID:
        str = "connection refused - invalid client identifier";
        break;

    case CONN_REF_SERVER_NOAVAIL:
        str = "connection refused - server unavailable";
        break;

    case CONN_REF_BAD_LOGIN:
        str = "connection refused - bad username or password";
        break;

    case CONN_REF_NO_AUTH:
        str = "connection refused - not authorised";
        break;

    case CONN_REF_BAD_TLS:
        str = "connection refused - TLS error";
        break;
    default:
        break;
    }

    lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->connect_ref);

    lua_pushboolean(ctx->L, success);
    lua_pushinteger(ctx->L, rc);
    lua_pushstring(ctx->L, str);

    if (lua_pcall(ctx->L, 3, 0, 0) != LUA_OK)
        say_error("Connect callback failed: ref:%d, message: \"%s\"",
                ctx->connect_ref, lua_tostring(ctx->L, -1));
}

static void
mosq_disconnect_f(struct mosquitto *mosq __attribute__((unused)),
        void *obj, int rc)
{
    mosq_t *ctx = obj;
    if (!ctx || !ctx->mosq)
        return;
    bool success = true;
    char *str = "client-initiated disconnect";

    if (rc) {
        success = false;
        str = "unexpected disconnect";
    }

    lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->disconnect_ref);

    lua_pushboolean(ctx->L, success);
    lua_pushinteger(ctx->L, rc);
    lua_pushstring(ctx->L, str);

    if (lua_pcall(ctx->L, 3, 0, 0) != LUA_OK)
        say_error("Disconnect callback failed: ref:%d, message: \"%s\"",
                ctx->disconnect_ref, lua_tostring(ctx->L, -1));

}

static void
mosq_publish_f(struct mosquitto *mosq __attribute__((unused)),
        void *obj, int mid)
{
    mosq_t *ctx = obj;
    if (!ctx || !ctx->mosq)
        return;
    lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->publish_ref);
    lua_pushinteger(ctx->L, mid);
    if (lua_pcall(ctx->L, 1, 0, 0) != LUA_OK)
        say_error("Publish callback failed: ref:%d, message: \"%s\"",
                ctx->publish_ref, lua_tostring(ctx->L, -1));

}

static void
mosq_message_f(struct mosquitto *mosq __attribute__((unused)),
        void *obj, const struct mosquitto_message *msg)
{
    mosq_t *ctx = obj;
    if (!ctx || !ctx->mosq)
        return;
    lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->message_ref);

    /**
     * function F(mid, topic, payload, qos, retain)
     */
    lua_pushinteger(ctx->L, msg->mid);
    lua_pushstring(ctx->L, msg->topic);
    lua_pushlstring(ctx->L, msg->payload, msg->payloadlen);
    lua_pushinteger(ctx->L, msg->qos);
    lua_pushboolean(ctx->L, msg->retain);

    if (lua_pcall(ctx->L, 5, 0, 0) != LUA_OK)
        say_error("Message callback failed: ref:%d, message: \"%s\"",
                ctx->message_ref, lua_tostring(ctx->L, -1));
}

static void
mosq_subscribe_f(struct mosquitto *mosq __attribute__((unused)),
        void *obj, int mid, int qos_count, const int *granted_qos)
{
    mosq_t *ctx = obj;
    if (!ctx || !ctx->mosq)
        return;
    lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->subscribe_ref);
    lua_pushinteger(ctx->L, mid);
    for (int i = 0; i < qos_count; i++)
        lua_pushinteger(ctx->L, granted_qos[i]);
    if (lua_pcall(ctx->L, qos_count + 1, 0, 0) != LUA_OK)
        say_error("Subscribe callback failed: ref:%d, message: \"%s\"",
                ctx->subscribe_ref, lua_tostring(ctx->L, -1));
}

static void
mosq_unsubscribe_f(struct mosquitto *mosq __attribute__((unused)),
        void *obj, int mid)
{
    mosq_t *ctx = obj;
    if (!ctx || !ctx->mosq)
        return;
    lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->unsubscribe_ref);
    lua_pushinteger(ctx->L, mid);
    if (lua_pcall(ctx->L, 1, 0, 0) != LUA_OK)
        say_error("Unsubscribe callback failed: ref:%d, message: \"%s\"",
                ctx->unsubscribe_ref, lua_tostring(ctx->L, -1));
}

static void
mosq_log_f(struct mosquitto *mosq __attribute__((unused)),
        void *obj, int level, const char *message)
{
    mosq_t *ctx = obj;
    if (!ctx || !ctx->mosq)
        return;
    if (ctx->log_level_mask & level) {
        lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->log_ref);
        lua_pushinteger(ctx->L, level);
        lua_pushstring(ctx->L, message);
        if (lua_pcall(ctx->L, 2, 0, 0) != LUA_OK)
            say_error("Loc callback failed: ref:%d, message: \"%s\"",
                ctx->log_ref, lua_tostring(ctx->L, -1));
    }
}

static int
mosq_log_callback_set(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);
    int log_level_mask = luaL_optinteger(L, 2, MOSQ_LOG_ERR);
    if (!lua_isfunction(L, 3))
        return luaL_argerror(L, 3, "expecting a function");

    ctx->log_level_mask = log_level_mask;
    ctx->log_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    mosquitto_log_callback_set(ctx->mosq, mosq_log_f);

    return make_mosq_status_result(L, MOSQ_ERR_SUCCESS);
}

static int
mosq_callback_set(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);

    enum callback_types callback_type = luaL_checkinteger(L, 2);

    if (!lua_isfunction(L, 3))
        return luaL_argerror(L, 3, "expecting a function");

    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    switch (callback_type) {
    case CONNECT:
        ctx->connect_ref = ref;
        mosquitto_connect_callback_set(ctx->mosq, mosq_connect_f);
        break;

    case DISCONNECT:
        ctx->disconnect_ref = ref;
        mosquitto_disconnect_callback_set(ctx->mosq, mosq_disconnect_f);
        break;

    case PUBLISH:
        ctx->publish_ref = ref;
        mosquitto_publish_callback_set(ctx->mosq, mosq_publish_f);
        break;

    case MESSAGE:
        ctx->message_ref = ref;
        mosquitto_message_callback_set(ctx->mosq, mosq_message_f);
        break;

    case SUBSCRIBE:
        ctx->subscribe_ref = ref;
        mosquitto_subscribe_callback_set(ctx->mosq, mosq_subscribe_f);
        break;

    case UNSUBSCRIBE:
        ctx->unsubscribe_ref = ref;
        mosquitto_unsubscribe_callback_set(ctx->mosq, mosq_unsubscribe_f);
        break;

    default:
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        luaL_argerror(L, 2, "unknown mosquitto callback type");
        break;
    }

    return make_mosq_status_result(L, MOSQ_ERR_SUCCESS);
}

static int
mosq_io_run_one(lua_State *L)
{
    mosq_t *ctx = mosq_get(L, 1);
    if (!ctx || !ctx->mosq) {
        say_error("mosq_io_run_one() called but object was destroyed");
        return 0;
    }
    int max_packets = luaL_optinteger(L, 2, 1);
    return make_mosq_status_result(L, mosq_do_io_run_one(ctx, max_packets));
}

/*
 * List of exporting: aliases, callbacks, definitions, functions etc [[
 */
struct define {
    const char* name;
    int value;
};

static const struct define defines[] = {

    {"CONNECT",     CONNECT},
    {"DISCONNECT",  DISCONNECT},
    {"PUBLISH",     PUBLISH},
    {"MESSAGE",     MESSAGE},
    {"SUBSCRIBE",   SUBSCRIBE},
    {"UNSUBSCRIBE", UNSUBSCRIBE},

    /** Log levels [[
     */
    {"LOG_NONE",    MOSQ_LOG_NONE},
    {"LOG_INFO",    MOSQ_LOG_INFO},
    {"LOG_NOTICE",  MOSQ_LOG_NOTICE},
    {"LOG_WARNING", MOSQ_LOG_WARNING},
    {"LOG_ERROR",   MOSQ_LOG_ERR},
    {"LOG_DEBUG",   MOSQ_LOG_DEBUG},
    {"LOG_ALL",     MOSQ_LOG_ALL},
    /** ]]
     */

    {NULL, 0}
};

/*
 * Lists of exporting: object and/or functions to the Lua
 */

static const struct luaL_Reg R[] = {
    {"version", mosq_lib_version},
    {"init",    mosq_lib_init},
    {"lib_destroy", mosq_lib_destroy},
    {"new",     mosq_new},
    {NULL, NULL}
};

static const struct luaL_Reg M[] = {

    {"destroy", mosq_destroy},

    /** Setup, options, misc [[
     */
    {"will_set",         mosq_will_set},
    {"will_clear",       mosq_will_clear},
    {"login_set",        mosq_login_set},
    {"tls_insecure_set", mosq_tls_insecure_set},
    {"tls_set",          mosq_tls_set},
    /* ]]
     */

    /** Events [[
     */
    {"publish",          mosq_publish},
    {"subscribe",        mosq_subscribe},
    {"unsubscribe",      mosq_unsubscribe},
    {"callback_set",     mosq_callback_set},
    {"log_callback_set", mosq_log_callback_set},
    /* ]]
     */

    /** Networking, coio loop [[
     */
    {"connect",    mosq_connect},
    {"reconnect",  mosq_reconnect},
    {"disconnect", mosq_disconnect},
    {"io_run_one", mosq_io_run_one},
    /** ]]
     */

    {NULL, NULL}
};
/*
 * ]]
 */


/*
 * Lib initializer
 */
LUA_API int
luaopen_mqtt_driver(lua_State *L)
{
    mosquitto_lib_init();
    mosq_initialized = true;

    /**
     * Add metatable.__index = metatable
     */
    luaL_newmetatable(L, MOSQ_LUA_UDATA_NAME);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_register(L, NULL, M);
    luaL_register(L, NULL, R);

    /**
     * Add definitions
     */
    const struct define *defs = &defines[0];
    while (defs->name) {
        lua_pushinteger(L, defs->value);
        lua_setfield(L, -2, defs->name);
        defs++;
    }

    return 1;
}
