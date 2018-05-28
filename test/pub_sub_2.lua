#!/usr/bin/env tarantool

-- Those lines of code are for debug purposes only
-- So you have to ignore them
-- {{
package.preload['mqtt.driver'] = './mqtt/driver.so'
-- }}

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

local stat = {
  on_subscribe = 0,
  on_message = 0,
  pulish_called = 0,
  published = 0
}

-- Subscriber
fiber.create(function()
  local sub = mqtt.new()

  F(sub:on_subscribe(function(mid, QoS)
    stat.on_subscribe = stat.on_subscribe + 1
  end))

  F(sub:on_message(function(mid, topic, payload, qos, retain)
      stat.on_message = stat.on_message + 1
      stat[topic .. '_' .. payload] = {mid=mid}
  end))

  F(sub:tls_insecure_set(false))
  F(sub:login_set('guest', 'guest'))
  F('connect', sub:connect({host="127.0.0.1", port=1883}))
  F(sub:subscribe('/'))

  -- Join
  fiber.sleep(5.0)

  print('Subscriber', yaml.encode(stat))
  assert(stat.on_subscribe == 1)
  assert(stat.published == stat.pulish_called)
  assert(stat.on_message == 1000)
  os.exit(0)
end)

-- Publisher
local conn = mqtt.new()
F(conn:on_publish(function(mid)
  stat.published = stat.published + 1
end))
F(conn:tls_insecure_set(false))
F(conn:login_set('guest', 'guest'))
F('connect', conn:connect({host="127.0.0.1", port=1883}))
for i = 1, 1000 do
  F(conn:publish('/', 'data_1', 0, false))
  stat.pulish_called = stat.pulish_called + 1
end
