#!/usr/bin/env tarantool

local mqtt = require('mqtt')
local fiber = require('fiber')
local yaml = require('yaml')
local os = require('os')
local log = require('log')

box.cfg{wal_mode = 'none'}
fan_in = box.schema.space.create('fan_in', {if_not_exists = true})
fan_in:create_index('id')

conn = mqtt.new()

function die(msg)
  error('[-] ' .. msg)
  os.exit(1)
end

function W(ok, msg)
  if not ok then
    die(msg)
  end
  return ok, msg
end

function W2(ok, msg)
  if not ok then
    log.error(msg)
  end
  return ok, msg
end

local TIME = '' .. fiber.time()

W(conn:on_message(
  function(mid, topic, payload, gos, retain)
    W2(conn:unsubscribe('channel2/messages'))
    fan_in:auto_increment{topic, payload, gos, retain}
    W2(conn:publish('channel/messages', TIME))
end))

W(conn:connect({host="127.0.0.1", port=1883}))
W(conn:subscribe('channel/#'))
W(conn:subscribe('channel2/#'))

print('[+] Starting tnt loop ...')

W(conn:publish('channel2/messages', TIME, 0, false))

fiber.create(function()
  while true do

    if fan_in:len() > 10 then
      for k, v in pairs(fan_in:select{}) do

        if v[2] ~= 'channel/messages' and v[2] ~= 'channel2/messages' then
          die('expected channel{2}/messages, got ' .. v[2])
        end
        if v[3] ~= TIME then
          die('expected ' .. time .. ', got ' .. v[3])
        end
        if v[4] ~= 0 then
          die('expected 0, got ' .. v[4])
        end
        if v[5] ~= false then
          die('expected false, got ' .. v[5])
        end
        --os.exit(0)
        print('[+] Test -- OK', yaml.encode(v))
        fan_in:delete{v[1]}
      end
      os.exit(0)
    end
    fiber.sleep(0.0)
  end
end)
