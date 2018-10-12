--
--  Copyright (C) 2016 - 2018 Tarantool AUTHORS: please see AUTHORS file.
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

local mqtt_driver = require('mqtt.driver')
local fiber = require('fiber')
local log = require('log')
local json = require('json')

local mqtt_mt

--
--  Create a new mosquitto client instance.
--
--  Parameters:
--    id -            String to use as the client id. If NULL, a random client id
--                    will be generated. If id is NULL, clean_session must be true.
--    clean_session - set to true to instruct the broker to clean all messages
--                   and subscriptions on disconnect, false to instruct it to
--                   keep them. See the man page mqtt(7) for more details.
--                   Note that a client will never discard its own outgoing
--                   messages on disconnect. Calling <connect> or
--                   <reconnect> will cause the messages to be resent.
--                   Must be set to true if the id parameter is NULL.
--
--  Returns:
--     mqtt object<see mqtt_mt> or raise error
--
local new = function(id, clean_session)
  local mqtt = mqtt_driver.new(id, clean_session)
  local ok, version_string = mqtt.version()

  return setmetatable({VERSION = version_string,
                       RECONNECT_INTERVAL = 0.5,
                       POLL_INTERVAL = 0.0,

                       mqtt = mqtt,
                       lib_destroy = mqtt_driver.lib_destroy,
                       connected = false,
                       auto_reconect = true,
                       fiber = nil,

                       subscribers = {}, }, mqtt_mt)
end

mqtt_mt = {

  __index = {

    --
    -- Private functions
    --
    __connect = function(self, opts)
      local host = self:__opt_get_or_nil(opts, 'host')
      local port = self:__opt_get_or_nil(opts, 'port')
      local keepalive = self:__opt_get_or_nil(opts, 'keepalive')
      return self.mqtt:connect(host, port, keepalive)
    end,

    -- [Re]Connect until not connected [[
    __reconnect = function(self)
      while not self.connected do
        -- Reconnect
        self.connected, _ = self.mqtt:reconnect()
        -- Restoring subscribing
        if self.connected then
          for topic, opts in pairs(self.subscribers) do
            self.connected, _ = self.mqtt:subscribe(topic, opts.qos)
          end
        end
        fiber.sleep(self.RECONNECT_INTERVAL)
      end
    end,
    -- ]]

    -- BG work
    __io_loop = function(self)
      local mq = self.mqtt
      while true do
        fiber.testcancel()
        self.connected = mq:io_run_one()
        if not self.connected then
          if self.auto_reconect then
            self:__reconnect()
          else
            log.error(
              'mqtt: the client is not currently connected, error %s', emsg)
          end
        end
        fiber.sleep(self.POLL_INTERVAL)
      end
    end,

    -- libmosquitto's log hook
    __default_logger_set = function(self, opts)
      local log_mask = self:__opt_get_or_nil(opts, 'log_mask')
      return self.mqtt:log_callback_set(log_mask, function(level, message)
          log.error("mqtt[%s]:'%s'", level, message)
      end)
    end,

    -- Helpers [[
    __opt_get = function(self, opts, what, default)
      if opts[what] then
        return opts[what]
      end
      return default
    end,
    __opt_get_or_nil = function(self, opts, what)
      return self:__opt_get(opts, what)
    end,
    -- ]]

    --
    -- Public functions
    --

    --
    -- Connect to an MQTT broker.
    --
    -- Parameters:
    --  opts.host          - the hostname or ip address of the broker to connect to.
    --  opts.port          - the network port to connect to. Usually 1883.
    --  opts.keepalive     - the number of seconds after which the broker should send a PING
    --                       message to the client if no other messages have been exchanged
    --                       in that time.
    --  opts.log_mask      - LOG_NONE, LOG_INFO, LOG_NOTICE, LOG_WARNING,
    --                       LOG_ERROR[default], LOG_DEBUG, LOG_ALL
    --  opts.auto_reconect - true[default], false - auto reconnect on, off
    --
    -- Returns:
    --  true or false, emsg
    --
    connect = function(self, opts_)
      
      if type(self) ~= 'table' or self.mqtt == nil then
        error("mqtt: usage: mqtt:connect()")
      end

      local opts = opts_ or {}

      self.auto_reconect = self:__opt_get(opts, 'auto_reconect', true)

      -- Setup logger
      local ok, emsg = self:__default_logger_set(opts)
      if not ok then
        return ok, emsg
      end

      ok, emsg = self:__connect(opts)
      if ok then
        self.fiber = fiber.create(self.__io_loop, self)
      end

      return ok, emsg
    end,

    --
    -- Reconnect to a broker.
    --
    --  This function provides an easy way of reconnecting to a broker after a
    --  connection has been lost. It uses the values that were provided in the
    --  <connect> call. It must not be called before
    --  <connect>.
    --
    --  NOTE
    --    After reconnecting you must call<subscribe> for subscribing
    --    to a topics.
    --
    --  See
    --    <connect> <opts.auto_reconect>
    --
    reconnect = function(self)
      if type(self) ~= 'table' or self.mqtt == nil then
        error("mqtt: usage: mqtt:reconnect()")
      end
      return self.mqtt:reconnect()
    end,

    --
    -- Subscribe to a topic.
    --
    -- Parameters:
    --  sub -  the subscription pattern.
    --  qos -  the requested Quality of Service for this subscription.
    --
    -- Returns:
    --  true or false, integer mid or error message
    --
    subscribe = function(self, topic, qos)
      if type(self) ~= 'table' or self.mqtt == nil then
        error("mqtt: usage: mqtt:subscribe()")
      end
      local ok, mid_or_emsg = self.mqtt:subscribe(topic, qos)
      if ok then
        self.subscribers[topic] = {qos = qos}
      end
      return ok, mid_or_emsg
    end,

    --
    -- Unsubscribe from a topic.
    --
    -- Parameters:
    --  topic - the unsubscription pattern.
    --
    -- Returns:
    --  true or false, integer mid or error message
    --
    unsubscribe = function(self, topic)
      if type(self) ~= 'table' or self.mqtt == nil then
        error("mqtt: usage: mqtt:unsubscribe()")
      end
      local ok, emsg = self.mqtt:unsubscribe(topic)
      self.subscribers[topic] = nil
      return ok, emsg
    end,

    --
    -- Publish a message on a given topic.
    --
    -- Parameters:
    --  topic -      null terminated string of the topic to publish to.
    --  payload -    some data (e.g. number, string) or table.
    --  qos -        integer value 0, 1 or 2 indicating the Quality of Service to be
    --               used for the will. When you call the library as "mqtt = require('mqtt')" 
    --               can be used mqtt.QOS_0, mqtt.QOS_1 and mqtt.QOS_2 as a replacement for some strange
    --               digital variable
    --  retain -     set to true to make the will a retained message. You can also use the values
    --               mqtt.RETAIN and mqtt.NON_RETAIN to replace the unmarked variable
    --
    -- Returns:
    --   true or false, emsg, message id(e.g. MID) is referenced in the publish callback
    --
    publish = function(self, topic, payload, qos, retail)
      if type(self) ~= 'table' or self.mqtt == nil then
        error("mqtt: usage: mqtt:publish()")
      end
      local raw_payload = payload
      if type(payload) == 'table' then
        raw_payload = json.encode(payload)
      end
      return self.mqtt:publish(topic, raw_payload, qos, retail)
    end,

    --
    -- Configure will information for a mosquitto instance. By default, clients do
    -- not have a will.  This must be called before calling <connect>.
    --
    -- Parameters:
    --  topic -      the topic on which to publish the will.
    --  payload -    pointer to the data to send.
    --  qos -        integer value 0, 1 or 2 indicating the Quality of Service to be
    --               used for the will.
    --  retain -     set to true to make the will a retained message.
    --
    -- Returns:
    --  true or false, emsg
    --
    will_set = function(self, topic, payload, qos, retain)
      return self.mqtt:will_set(topic, payload, qos, retain)
    end,

    --
    --
    -- Remove a previously configured will. This must be called before calling
    -- <connect>.
    --
    -- Returns:
    --  true or false, emsg
    --
    will_clear = function(self)
      return self.mqtt:will_clear()
    end,

    --
    -- Configure username and password for a mosquitton instance. This is only
    -- supported by brokers that implement the MQTT spec v3.1. By default, no
    -- username or password will be sent.
    -- If username is NULL, the password argument is ignored.
    -- This must be called before calling connect.
    --
    -- This is must be called before calling <connect>.
    --
    -- Parameters:
    --  username - the username to send as a string, or NULL to disable
    --             authentication.
    --  password - the password to send as a string. Set to NULL when username is
    --             valid in order to send just a username.
    --
    -- Returns:
    --  true or false, emsg
    --
    login_set = function(self, username, password)
      if type(self) ~= 'table' or self.mqtt == nil then
        error("mqtt: usage: mqtt:login_set()")
      end
      return self.mqtt:login_set(username, password)
    end,

    --
    -- Configure verification of the server hostname in the server certificate. If
    -- value is set to true, it is impossible to guarantee that the host you are
    -- connecting to is not impersonating your server. This can be useful in
    -- initial server testing, but makes it possible for a malicious third party to
    -- impersonate your server through DNS spoofing, for example.
    -- Do not use this function in a real system. Setting value to true makes the
    -- connection encryption pointless.
    -- Must be called before <connect>.
    --
    -- Parameters:
    --  value - if set to false, the default, certificate hostname checking is
    --          performed. If set to true, no hostname checking is performed and
    --          the connection is insecure.
    --
    -- Returns:
    --  true or false, emsg
    --
    tls_insecure_set = function(self, value)
      return self.mqtt:tls_insecure_set(value)
    end,

    --
    -- Configure the client for certificate based SSL/TLS support. Must be called
    -- before <connect>.
    --
    -- Define the Certificate Authority certificates to be trusted (ie. the server
    -- certificate must be signed with one of these certificates) using cafile.
    --
    -- If the server you are connecting to requires clients to provide a
    -- certificate, define certfile and keyfile with your client certificate and
    -- private key. If your private key is encrypted, provide a password callback
    -- function or you will have to enter the password at the command line.
    --
    -- Parameters:
    --  cafile -      path to a file containing the PEM encoded trusted CA
    --                certificate files. Either cafile or capath must not be NULL.
    --  capath -      path to a directory containing the PEM encoded trusted CA
    --                certificate files. See mosquitto.conf for more details on
    --                configuring this directory. Either cafile or capath must not
    --                be NULL.
    --  certfile -    path to a file containing the PEM encoded certificate file
    --                for this client. If NULL, keyfile must also be NULL and no
    --                client certificate will be used.
    --  keyfile -     path to a file containing the PEM encoded private key for
    --                this client. If NULL, certfile must also be NULL and no
    --                client certificate will be used.
    --  pw_callback - TODO Implement me
    --
    -- Returns:
    --  true or false, emsg
    --
    -- See Also:
    --   <insecure_set>
    --
    tls_set = function(self, cafile, capath, certfile, keyfile)
      return self.mqtt:tls_set(cafile, capath, certfile, keyfile)
    end,

    --
    -- Subscribe on events
    --

    --
    -- Set the connect callback. This is called when the broker sends a CONNACK
    -- message in response to a connection.
    --
    -- Parameters:
    --  F - a callback function in the following form:
    --      function F(boolean_success, integer_ret_code, string_message)
    --
    -- Returns:
    --  true or false, emsg
    --
    on_connect = function(self, F)
      return self.mqtt:callback_set(mqtt_driver.CONNECT, F)
    end,

    --
    -- Set the publish callback. This is called when a message initiated with
    -- <publish> has been sent to the broker successfully.
    --
    -- Parameters:
    --  F - a callback function in the following form:
    --      function F(integer_mid)
    --      integer_mid - the message id of the sent message.
    --
    -- Returns:
    --  true or false, emsg
    --
    on_publish = function(self, F)
      return self.mqtt:callback_set(mqtt_driver.PUBLISH, F)
    end,

    --
    -- Set the subscribe callback. This is called when the broker responds to a
    -- subscription request.
    --
    -- Parameters:
    --  F - a callback function in the following form:
    --      function F(integer_mid, array_of_qos)
    --      integer_mid - the message id of the subscribe message.
    --      array_of_qos - an array of integers indicating the granted QoS for
    --                     each of the subscriptions.
    -- Returns:
    --  true or false, emsg
    --
    on_subscribe = function(self, F)
      return self.mqtt:callback_set(mqtt_driver.SUBSCRIBE, F)
    end,

    --
    -- Set the unsubscribe callback. This is called when the broker responds to a
    -- unsubscription request.
    --
    -- Parameters:
    -- F - a callback function in the following form:
    --     function F(integer_mid)
    --     mid -  the message id of the unsubscribe message.
    --
    -- Returns:
    --  true or false, emsg
    --
    on_unsubscribe = function(self, F)
      return self.mqtt:callback_set(mqtt_driver.UNSUBSCRIBE, F)
    end,

    --
    -- Set the disconnect callback. This is called when the broker has received the
    -- DISCONNECT command and has disconnected the client.
    --
    -- Parameters:
    --  on_disconnect - a callback function in the following form:
    --                  function F(boolean_success, integer_ret_code, string_message)
    --
    -- Returns:
    --  true or false, emsg
    --
    on_disconnect = function(self, F)
      return self.mqtt:callback_set(mqtt_driver.DISCONNECT, F)
    end,

    --
    -- Set the message callback. This is called when a message is received
    -- from the broker.
    --
    -- Parameters:
    --  F - a callback function in the following form:
    --      function F(integer_mid, string_topic, string_payload, integer_qos, integer_retain)
    --
    -- Returns:
    --  true or false, emsg
    --
    on_message = function(self, F)
      return self.mqtt:callback_set(mqtt_driver.MESSAGE, F)
    end,

    --
    -- Destroy (i.e. free) self
    --
    destroy = function(self)
      if type(self) ~= 'table' or self.mqtt == nil then
        error("mqtt: usage: mqtt:destroy()")
      end
      if (self.fiber ~= nil and self.fiber.cancel ~= nil) then
        self.fiber:cancel()
      end
      local ok, emsg = self.mqtt:destroy()
      self = nil
      return ok, emsg
    end,
  },
}

--
-- Export
--
return {
  new = new,
  QOS_0 = 0,
  QOS_1 = 1,
  QOS_2 = 2,
  RETAIN = true,
  NON_RETAIN = false
}
