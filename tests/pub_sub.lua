#!/usr/bin/env tarantool

local mqtt = require("mqtt")

conn = mqtt.new()

ok, msg = conn:on_message(function(mid, topic, payload)
  print(mid, topic, payload)
end)

ok, msg = conn:connect({host="localhost", port=9999},
function()
  print('connected')
  ok, mid = conn:subscribe('hello')
  print(ok, mid)
  ok, mid = conn:subscribe('hello 1')
  print(ok, mid)
end)
