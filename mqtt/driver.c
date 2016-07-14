/*
 * Copyright (C) 2016 Tarantool AUTHORS: please see AUTHORS file.
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

#include "mosq.h"

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


/* handle mosquitto lib return codes */
static int
mosq_lib_version(lua_State *L)
{
	int major, minor, rev;
	char version[16];

	mosquitto_lib_version(&major, &minor, &rev);
	sprintf(version, "%i.%i.%i", major, minor, rev);
	lua_pushstring(L, version);
	return 1;
}

static int
mosq_lib_init(lua_State *L)
{
	if (!mosq_initialized)
		mosquitto_lib_init();

	return make_mosq_status_result(L, MOSQ_ERR_SUCCESS);
}

static int
mosq_lib_cleanup(lua_State *L)
{
	mosquitto_lib_cleanup();
	mosq_initialized = false;
	return make_mosq_status_result(L, MOSQ_ERR_SUCCESS);
}

static int
mosq_new(lua_State *L)
{
	const char *id = luaL_optstring(L, 1, NULL);
	bool clean_session = (lua_isboolean(L, 2) ? lua_toboolean(L, 2) : true);

	if (id == NULL && !clean_session) {
		return luaL_argerror(L, 2,
				"if 'id' is nil then 'clean session' must be true");
	}

	mosq_t *ctx = (mosq_t *) lua_newuserdata(L, sizeof(mosq_t));
	if (!ctx) {
		return luaL_error(L, "lua_newuserdata failed!");
	}

	mosq_init(ctx);

	/* ctx will be passed as void *obj arg in the callback functions */
	ctx->mosq = mosquitto_new(id, clean_session, ctx);

	if (ctx->mosq == NULL) {
		return luaL_error(L, strerror(errno));
	}

	ctx->L = L;

	luaL_getmetatable(L, MOSQ_LUA_UDATA_NAME);
	lua_setmetatable(L, -2);

	return 1;
}

static int
mosq_destroy(lua_State *L)
{
	mosq_t *ctx = mosq_get(L, 1);

	mosq_free(ctx);

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

	if (!lua_isnil(L, 3)) {
		payload = lua_tolstring(L, 3, &payloadlen);
	}

	int qos = luaL_optinteger(L, 4, 0);
	bool retain = lua_toboolean(L, 5);

	int rc = mosquitto_will_set(
		ctx->mosq, topic, payloadlen, payload,
		qos, retain);

	return make_mosq_status_result(L, rc);
}

static int
mosq_will_clear(lua_State *L)
{
	mosq_t *ctx = mosq_get(L, 1);

	int rc = mosquitto_will_clear(ctx->mosq);

	return make_mosq_status_result(L, rc);
}

static int
mosq_login_set(lua_State *L)
{
	mosq_t *ctx = mosq_get(L, 1);
	const char *username = (lua_isnil(L, 2) ? NULL : luaL_checkstring(L, 2));
	const char *password = (lua_isnil(L, 3) ? NULL : luaL_checkstring(L, 3));

	int rc = mosquitto_username_pw_set(ctx->mosq, username, password);

	return make_mosq_status_result(L, rc);
}

static int
mosq_tls_set(lua_State *L)
{
	mosq_t *ctx = mosq_get(L, 1);
	const char *cafile = luaL_optstring(L, 2, NULL);
	const char *capath = luaL_optstring(L, 3, NULL);
	const char *certfile = luaL_optstring(L, 4, NULL);
	const char *keyfile = luaL_optstring(L, 5, NULL);

	// the last param is a callback to a function that asks for a passphrase for a keyfile
	// our keyfiles should NOT have a passphrase
	int rc = mosquitto_tls_set(ctx->mosq, cafile, capath, certfile, keyfile, 0);

	return make_mosq_status_result(L, rc);
}

static int
mosq_tls_insecure_set(lua_State *L)
{
	mosq_t *ctx = mosq_get(L, 1);
	bool value = lua_toboolean(L, 2);

	int rc = mosquitto_tls_insecure_set(ctx->mosq, value);
	return make_mosq_status_result(L, rc);
}

static int
mosq_connect(lua_State *L)
{
	mosq_t *ctx = mosq_get(L, 1);
	const char *host = luaL_optstring(L, 2, "localhost");
	int port = luaL_optinteger(L, 3, 1883);
	int keepalive = luaL_optinteger(L, 4, 60);

	int rc = mosquitto_connect(ctx->mosq, host, port, keepalive);
	return make_mosq_status_result(L, rc);
}

static int
mosq_reconnect(lua_State *L)
{
	mosq_t *ctx = mosq_get(L, 1);

	int rc = mosquitto_reconnect(ctx->mosq);
	return make_mosq_status_result(L, rc);
}

static int
mosq_disconnect(lua_State *L)
{
	mosq_t *ctx = mosq_get(L, 1);

	int rc = mosquitto_disconnect(ctx->mosq);
	return make_mosq_status_result(L, rc);
}

static int
mosq_publish(lua_State *L)
{
	mosq_t *ctx = mosq_get(L, 1);
	int mid;	/* message id is referenced in the publish callback */
	const char *topic = luaL_checkstring(L, 2);
	size_t payloadlen = 0;
	const void *payload = NULL;

	if (!lua_isnil(L, 3)) {
		payload = lua_tolstring(L, 3, &payloadlen);
	};

	int qos = luaL_optinteger(L, 4, 0);
	bool retain = lua_toboolean(L, 5);

	int rc = mosquitto_publish(ctx->mosq, &mid, topic, payloadlen, payload, qos, retain);

	if (rc != MOSQ_ERR_SUCCESS) {
		return make_mosq_status_result(L, rc);
	}

	return make_int_result(L, true, mid);
}

static int
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

static int
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
mosq_connect_f(
	struct mosquitto *mosq,
	void *obj,
	int rc)
{
	(void)mosq;

	mosq_t *ctx = obj;
	enum connect_return_codes return_code = rc;

	bool success = false;
	char *str = "connection status unknown";

	switch(return_code) {
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

	lua_call(ctx->L, 3, 0);
}


static void
mosq_disconnect_f(
	struct mosquitto *mosq,
	void *obj,
	int rc)
{
	(void)mosq;

	mosq_t *ctx = obj;
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

	lua_call(ctx->L, 3, 0);
}

static void
mosq_on_publish(
	struct mosquitto *mosq,
	void *obj,
	int mid)
{
	(void)mosq;

	mosq_t *ctx = obj;

	lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->publish_ref);
	lua_pushinteger(ctx->L, mid);
	lua_call(ctx->L, 1, 0);
}

static void
mosq_message_f(
	struct mosquitto *mosq,
	void *obj,
	const struct mosquitto_message *msg)
{
	(void)mosq;

	mosq_t *ctx = obj;

	/* push registered Lua callback function onto the stack */
	lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->message_ref);
	/* push function args */
	lua_pushinteger(ctx->L, msg->mid);
	lua_pushstring(ctx->L, msg->topic);
	lua_pushlstring(ctx->L, msg->payload, msg->payloadlen);
	lua_pushinteger(ctx->L, msg->qos);
	lua_pushboolean(ctx->L, msg->retain);

	lua_call(ctx->L, 5, 0); /* args: mid, topic, payload, qos, retain */
}

static void
mosq_subscribe_f(
	struct mosquitto *mosq,
	void *obj,
	int mid,
	int qos_count,
	const int *granted_qos)
{
	(void)mosq;

	mosq_t *ctx = obj;

	lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->subscribe_ref);
	lua_pushinteger(ctx->L, mid);

	for (int i = 0; i < qos_count; i++) {
		lua_pushinteger(ctx->L, granted_qos[i]);
	}

	lua_call(ctx->L, qos_count + 1, 0);
}

static void
mosq_unsubscribe_f(
	struct mosquitto *mosq,
	void *obj,
	int mid)
{
	(void)mosq;

	mosq_t *ctx = obj;

	lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->unsubscribe_ref);
	lua_pushinteger(ctx->L, mid);
	lua_call(ctx->L, 1, 0);
}

static void
mosq_log_f(
	struct mosquitto *mosq,
	void *obj,
	int level,
	const char *message)
{
	(void)mosq;

	mosq_t *ctx = obj;

	if (ctx->log_level_mask & level) {
		lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->log_ref);

		lua_pushinteger(ctx->L, level);
		lua_pushstring(ctx->L, message);

		lua_call(ctx->L, 2, 0);
	}
}

static int
mosq_log_callback_set(lua_State *L)
{
	mosq_t *ctx = mosq_get(L, 1);

	int log_level_mask = luaL_optinteger(L, 2, MOSQ_LOG_ALL);

	if (!lua_isfunction(L, 3)) {
		return luaL_argerror(L, 3, "expecting a function");
	}

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

	if (!lua_isfunction(L, 3)) {
		return luaL_argerror(L, 3, "expecting a function");
	}

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
		mosquitto_publish_callback_set(ctx->mosq, mosq_on_publish);
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
mosq_poll_one(lua_State *L)
{
	mosq_t *ctx = mosq_get(L, 1);
	int revents = luaL_optinteger(L, 2, COIO_READ|COIO_WRITE);
	int timeout = luaL_optinteger(L, 3, TIMEOUT_INFINITY);
	int mosq_max_packets = luaL_optinteger(L, 4, 1);

	/** XXX
	 * I'm confused: socket < 0 means MOSQ_ERR_NO_CONN
	 */
	int rc = MOSQ_ERR_NO_CONN;

	int fd = mosquitto_socket(ctx->mosq);

	if (fd >= 0) {

		/** Wait until event
		 */
		revents = coio_wait(fd, revents, timeout);

		if (revents != 0) {
			if (revents & COIO_READ)
				rc = mosquitto_loop_read(ctx->mosq, mosq_max_packets);
			if (revents & COIO_WRITE)
				rc = mosquitto_loop_write(ctx->mosq, mosq_max_packets);
		}

		/**
		 * mosquitto_loop_miss
		 * This function deals with handling PINGs and checking
		 * whether messages need to be retried,
		 * so should be called fairly _frequently_(!).
		 * */
		if (ctx->next_misc_timeout < fiber_time64()) {
			rc = mosquitto_loop_misc(ctx->mosq);
			ctx->next_misc_timeout = fiber_time64() + 1200;
		}
	}

	return make_mosq_status_result(L, rc);
}

/*
 * List of exporting: aliases, callbacks, definitions, functions etc [[
 */
struct define {
	const char* name;
	int value;
};

static const struct define main_defs[] = {

	{"CONNECT",		CONNECT},
	{"DISCONNECT",	DISCONNECT},
	{"PUBLISH",		PUBLISH},
	{"MESSAGE",		MESSAGE},
	{"SUBSCRIB",	SUBSCRIBE},
	{"UNSUBSCRIBE",	UNSUBSCRIBE},

	/** Log levels [[
	 */
	{"LOG_NONE",	MOSQ_LOG_NONE},
	{"LOG_INFO",	MOSQ_LOG_INFO},
	{"LOG_NOTICE",	MOSQ_LOG_NOTICE},
	{"LOG_WARNING",	MOSQ_LOG_WARNING},
	{"LOG_ERROR",	MOSQ_LOG_ERR},
	{"LOG_DEBUG",	MOSQ_LOG_DEBUG},
	{"LOG_ALL",		MOSQ_LOG_ALL},
	/** ]]
	 */

	{NULL,			0}
};

static void
mosq_register_defs(lua_State *L, const struct define *defs)
{
	while (defs->name) {
		lua_pushinteger(L, defs->value);
		lua_setfield(L, -2, defs->name);
		defs++;
	}
}

/*
 * Lists of exporting: object and/or functions to the Lua
 */

static const struct luaL_Reg R[] = {
	{"version", mosq_lib_version},
	{"init",    mosq_lib_init},
	{"cleanup", mosq_lib_cleanup},
	{"__gc",    mosq_lib_cleanup},
	{"new",     mosq_new},
	{NULL,      NULL}
};

static const struct luaL_Reg M[] = {

	{"destroy", mosq_destroy},
	{"__gc",    mosq_destroy},

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
	{"poll_one",   mosq_poll_one},
	/** ]]
	 */

	{NULL,		NULL}
};

/*
 * ]]
 */


/*
 * Lib initializer
 */
LUA_API int
luaopen_mqtt(lua_State *L)
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
	mosq_register_defs(L, main_defs);

	return 1;
}
