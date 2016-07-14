--
--  Copyright (C) 2016 Tarantool AUTHORS: please see AUTHORS file.
--
--  Redistribution and use in source and binary forms, with or
--  without modification, are permitted provided that the following
--  conditions are met:
--
--  1. Redistributions of source code must retain the above
--     copyright notice, this list of conditions and the
--     following disclaimer.
--
--  2. Redistributions in binary form must reproduce the above
--     copyright notice, this list of conditions and the following
--     disclaimer in the documentation and/or other materials
--     provided with the distribution.
--
--  THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
--  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
--  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
--  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
--  <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
--  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
--  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
--  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
--  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
--  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
--  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
--  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
--  SUCH DAMAGE.
--

local mqtt = require('mqtt.driver')
local fiber = require('fiber')
local log = require('log')

--box.cfg{wal_mode = 'none'}

local conn_mt

--[[
  opts = {
    host
    port
  }
--]]
local function new()
  return setmetatable({
    -- Fields
    mqtt = mqtt.new(),
    fiber = nil,
    connected = false,
    auto_reconect = true,
  }, conn_mt)
end

conn_mt = {

  __index = {

    -- Fields [[
    -- ]]

    -- Private functions [[
    __try_connect = function(self, hots, port, keepalive)
      while not self.connected do
        self.connected, _ = self.mqtt:connect(host, port, keepalive)
        print (self.connect, _)
        fiber.sleep(0.5)
      end
    end,

    __try_reconnect = function(self)
      while not self.connected do
        self.connected, _ = self.mqtt:reconnect()
        fiber.sleep(0.5)
      end
    end,

    __poll_forever = function(self)
      local mq = self.mqtt
      while true do
        self.connected, _ = mq:poll_one()
        if self.auto_reconect and not self.connected then
          self:__try_reconnect()
        end
        fiber.sleep(0.0)
      end
    end,

    __default_logger_set = function(self, log_mask)
      return self.mqtt:log_callback_set(log_mask, function(level, message)
          log.error("mqtt[%s]:'%s'", level, message)
      end)
    end,
    -- ]]

    connect = function(self, opts_, on_connect_F)

      local opts = opts_ or {}

      self.auto_reconect = opts.auto_reconect or true

      local ok, emsg

      -- Setup logger
      ok, emsg = self:__default_logger_set(opts.log_mask)
      if not ok then
        return ok, emsg
      end

      ok, emsg = self:on_connect(on_connect_F)
      if not ok then
        return ok, emsg
      end

      -- Start work
      self.fiber = fiber.create(
          function()
            self:__try_connect(opts.host, opts.port, opts.keepalive)
            self:__poll_forever()
          end
      )

      return true
    end,

    -- Subscribe on events [[
    on_connect = function(self, F)
      return self.mqtt:callback_set(mqtt.CONNECT, F)
    end,

    on_publish = function(self, F)
      return self.mqtt:callback_set(mqtt.PUBLISH, F)
    end,

    on_subscribe = function(self, F)
      return self.mqtt:callback_set(mqtt.SUBSCRIBE, F)
    end,

    on_unsubscribe = function(self, F)
      return self.mqtt:callback_set(mqtt.UNSUBSCRIBE, F)
    end,

    on_disconnect = function(self, F)
      return self.mqtt:callback_set(mqtt.DISCONNECT, F)
    end,

    on_message = function(self, F)
      return self.mqtt:callback_set(mqtt.MESSAGE, F)
    end,
    -- ]]

    subscribe = function(self, channel)
      return self.mqtt:subscribe(channel)
    end,

    publish = function(self, topic, message)
      return self.mqtt:publish(topic, message)
    end,
  },
}

return {
  new = new,
}
