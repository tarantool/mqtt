package.preload['mqtt.driver'] = './mqtt/driver.so'

local os = require('os')

local mqtt = require('mqtt')

conn = mqtt.new()

conn:connect({host="127.0.0.1", port=1883})
conn:reconnect()
conn:subscribe('channel/#')
conn:unsubscribe('some/topic')
conn:publish('channel2/messages', TIME, 0, false)
conn:will_set('some/topic', 'str', 0, false)
conn:will_clear()
conn:login_set('user', 'password')
conn:tls_insecure_set(false)
--conn:tls_set('file', 'path', 'cert', 'key')
conn:on_connect(function() end)
conn:on_publish(function() end)
conn:on_subscribe(function() end)
conn:on_unsubscribe(function() end)
conn:on_disconnect(function() end)
conn:on_message(function() end)

os.exit(0)
