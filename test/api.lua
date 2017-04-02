#!/usr/bin/env tarantool

-- Those lines of code are for debug purposes only
-- So you have to ignore them
-- {{
package.preload['mqtt.driver'] = './mqtt/driver.so'
-- }}

box.cfg {wal_mode = 'none'}

local os = require('os')
local mqtt = require('mqtt')
local yaml = require('yaml')
local fiber = require('fiber')

local function F(...)
  local desc, ok, emsg = ...
  if not ok then
    error(string.format("%s failed. Message = %s", desc, emsg))
  end
  return ...
end

local function E(...)
  local desc, ok, emsg = ...
  if ok then
    error(string.format("%s it should be failed. Message = %s", desc, emsg))
  end
  return ...
end

local stat = { }
local function stat_inc(key)
  if stat[key] == nil then
    stat[key] = 0
  end
  stat[key] = stat[key] + 1
end

local conn = mqtt.new()

assert(type(conn.VERSION) == 'string')
assert(#conn.VERSION > 5)

F(conn:on_connect(function(success, ret_code, message)
  if not success then
    print("can't connect msg = %s, ret_code = %d", message, ret_code)
    os.exit(1)
  end
  stat_inc('on_connect')
end))

F(conn:on_publish(function(mid)
  stat_inc('on_publish')
end))

F(conn:on_subscribe(function(mid, QoS)
  stat_inc('on_subscribe')
end))

F(conn:on_unsubscribe(function(mid)
  stat_inc('on_unsubscribe')
end))

F(conn:on_disconnect(
  function(success, ret_code, message)
    if not success then
      print("can't connect msg = %s, ret_code = %d", message, ret_code)
      os.exit(1)
    end
    stat_inc('on_disconnect')
  end
))

F(conn:on_message(
  function(mid, topic, payload, qos, retain)
    stat_inc('on_message')
    stat[topic .. '_' .. payload] = {mid=mid}
  end
))

-- https://github.com/tarantool/mqtt/issues/6 [[
E('connect', conn:connect({port=1889}))
E('connect', conn:connect({host="www.does.not.exist"}))
E('connect', conn:connect({host="127.0.0.1", port=1889}))
E('publish', conn:publish('channel/in_1', 'data_1', 0, false))
E('subscribe', conn:subscribe('channel/in_1'))
E('reconnect', conn:reconnect())
-- ]]

-- Here is setup for defaul Rabbitmq MQTT provider.
-- (!) Except. login_set [[
F(conn:tls_insecure_set(false))
F('connect', conn:connect({host="127.0.0.1", port=1883}))
F(conn:login_set('user', 'password'))
-- ]]

F('reconnect', conn:reconnect())

F(conn:subscribe('some/topic'))
F(conn:will_set('some/topic', 'str', 0, false))
F(conn:will_clear())
F(conn:unsubscribe('some/topic'))

F(conn:subscribe('channel/#'))
F(conn:publish('channel/in_1', 'data_1', 0, false))
F(conn:publish('channel/in_1', 'data_2', 0, false))
F(conn:publish('channel/in_2', 'data_1', 0, false))
F(conn:publish('channel/in_2', 'data_2', 0, false))
F(conn:unsubscribe('channel/#'))

-- Join
print ("[+] Join result")
fiber.sleep(2.0)

assert(stat.on_message == stat.on_publish)
assert(stat.on_connect == 1)
assert(stat.on_subscribe == 2)
assert(stat['channel/in_1_data_1'] ~= nil)
assert(stat['channel/in_1_data_2'] ~= nil)
assert(stat['channel/in_2_data_1'] ~= nil)
assert(stat['channel/in_2_data_2'] ~= nil)

print ("[+] Done")

os.exit(0)
