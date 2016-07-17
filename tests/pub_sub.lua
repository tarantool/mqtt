#!/usr/bin/env tarantool

local mqtt = require('mqtt')
local fiber = require('fiber')
local yaml = require('yaml')
local os = require('os')

box.cfg{wal_mode = 'none'}
fan_in = box.schema.space.create('fan_in', {if_not_exists = true})
fan_in:create_index('id')

conn = mqtt.new()

function W(ok, msg)
  if not ok then
    error('[-] ' .. msg)
  end
  return ok, msg
end

W(conn:on_message(
  function(mid, topic, payload, gos, retain)
    fan_in:auto_increment{topic, payload, gos, retain}
    print("[+] Message - OK")
end))

W(conn:connect({host="127.0.0.1", port=1883}))
W(conn:subscribe('channel/#'))
W(conn:subscribe('channel2/#'))

print('[+] Starting tnt loop ...')
fiber.create(function()
  local time = '' .. fiber.time()
  while true do
    W(conn:publish('channel/messages', time, 0, false))
    W(conn:publish('channel2/messages', time, 0, false))
    if fan_in:len() == 10 then
      for k, v in pairs(fan_in:select{}) do
        if v[2] ~= 'channel/messages' and v[2] ~= 'channel2/messages' then
          error('expected channel{2}/messages, got ' .. v[2])
        end
        if v[3] ~= time then
          error('expected ' .. time .. ', got ' .. v[3])
        end
        if v[4] ~= 0 then
          error('expected 0, got ' .. v[4])
        end
        if v[5] ~= false then
          error('expected false, got ' .. v[5])
        end
        print('[+] Test -- OK')
        os.exit(0)
      end
    end
    fiber.sleep(0.0)
  end
end)
