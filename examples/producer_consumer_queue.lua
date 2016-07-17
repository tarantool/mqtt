#!/usr/bin/env tarantool

local mqtt = require('mqtt')
local fiber = require('fiber')
local yaml = require('yaml')

box.cfg {wal_mode = 'none'}

-- Queue out
queue = box.schema.space.create('queue')
queue:create_index('id')

-- Create instance
connection = mqtt.new()

-- Connect to the server
local ok, emsg = connection:connect({host='127.0.0.1', port=1883})
if not ok then
  error('connect ->', emsg)
end

-- Subscribe to a system topic
local ok, emsg = connection:subscribe('tarantool/box/info')
if not ok then
  error('subscribe -> ',emsg)
end

--
-- Work
--

-- Consumer
connection:on_message(function (message_id, topic, payload, gos, retain)
  queue:auto_increment{yaml.decode(payload)}
  if queue:len() > 10 then
    print('Queue [[')
    for _, v in pairs(queue:select{}) do
      print(_, yaml.encode(v[2]))
      queue:delete(v[1])
    end
    print(']]')
  end
end)

-- Producer
fiber.create(function()
  while true do
    connection:publish('tarantool/box/info', yaml.encode{box.info()})
    fiber.sleep(0.5)
  end
end)
