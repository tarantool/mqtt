#!/usr/bin/env tarantool

-- Load Tarantool mqtt
local mqtt = require('mqtt')

-- Create instance
connection = mqtt.new()

-- Connect to the server
connection:connect({host='127.0.0.1', port=1883})

-- Set callback for recv new messages
connection:on_message(function (message_id, topic, payload, gos, retain)
  print('New message', message_id, topic, payload, gos, retain)
end)

-- Subscribe to a system topic
connection:subscribe('$SYS/#')
