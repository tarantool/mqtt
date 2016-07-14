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

#ifndef MOSQ_H
#define MOSQ_H 1

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <tarantool/module.h>

#include "utils.h"

/**
 * Unique name for userdata metatables
 */
#define MOSQ_LUA_UDATA_NAME	"__tnt_mqtt_mosquitto"

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

	uint64_t next_misc_timeout;
	int log_level_mask;
} mosq_t;

static inline mosq_t*
mosq_get(lua_State *L, int i)
{
	return (mosq_t *) luaL_checkudata(L, i, MOSQ_LUA_UDATA_NAME);
}

static inline void
mosq_init(mosq_t *ctx)
{
	ctx->connect_ref = LUA_REFNIL;
	ctx->disconnect_ref = LUA_REFNIL;
	ctx->publish_ref = LUA_REFNIL;
	ctx->message_ref = LUA_REFNIL;
	ctx->subscribe_ref = LUA_REFNIL;
	ctx->unsubscribe_ref = LUA_REFNIL;
	ctx->log_ref = LUA_REFNIL;

	ctx->next_misc_timeout = 0;
	ctx->log_level_mask = 0;
}

static inline void
mosq_lua_clear(mosq_t *ctx)
{
	luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->connect_ref);
	luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->disconnect_ref);
	luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->publish_ref);
	luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->message_ref);
	luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->subscribe_ref);
	luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->unsubscribe_ref);
	luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->log_ref);
	/* Set default values */
}

static inline void
mosq_free(mosq_t *ctx)
{
	if (ctx->mosq) {
		mosquitto_destroy(ctx->mosq);
		ctx->mosq = NULL;
	}

	mosq_lua_clear(ctx);
}

#endif
