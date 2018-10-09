<a href="https://travis-ci.org/tarantool/mqtt">
  <img src="https://travis-ci.org/tarantool/mqtt.png?branch=master">
</a>

# Tarantool MQTT client
---------------------------------

Key features:

* non-blocking communication with MQTT brokers;
* TLS support;
* pretty low overheads, code based on `libmosquitto`.

## Content
----------
* [Prerequisites](#prerequisites)
* [Building from source](#building-from-source)
* [API](#api)
  * [lib_destroy](#lib_destroy)
  * [new](#new)
  * [connect](#connect)
  * [reconnect](#reconnect)
  * [subscribe](#subscribe)
  * [unsubscribe](#unsubscribe)
  * [destroy](#destroy)
  * [publish](#publish)
  * [will_set](#will_set)
  * [will_clear](#will_clear)
  * [login_set](#login_set)
  * [tls_insecure_set](#tls_insecure_set)
  * [tls_set](#tls_set)
  * [on_message](#on_message)
  * [Subscribe on events](#Subscribe-on-events)
* [Performance tuning](#performance-tuning)
* [Examples](#examples)
* [Copyright & License](#copyright--license)
* [See also](#see-also)

## Prerequisites
-------------------------------

  Before reading any further, make sure you have an MQTT broker installed.

### Building from source

Clone the repository with submodules, bootstrap, and build the client:

```bash
$ git clone https://github.com/tarantool/mqtt.git
$ cd mqtt
$ git submodule update --init --recursive
$ ./bootstrap.sh
$ make -C build
```

[Back to content](#content)

## Api
------

Lua API documentation

### lib_destroy
---------------

  Cleanup everything.

  Note: The module does not use the Lua's GC and have to be called manually. 
  To call it manually, first call `destroy` on each `mqtt` object.

  Parameters:

    None

  Returns:

    None

  Example:

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
  mqtt.lib_destroy()
```

[Back to content](#content)

### new
-------

  Create a new `mosquitto` client instance.

  Parameters:

    client_id       - String. If NULL, a random client id will be generated 
                      and the clean_session parameter must be set to true.
    clean_session   - Boolean. Set to true to instruct the broker to clean all 
                      messages and subscriptions on disconnect; false to instruct 
                      it to keep them. See the man page mqtt(7) for more details.
                      Note that a client will never discard its own outgoing
                      messages on disconnect. Calling (connect)[#connect] or
                      (reconnect)[#reconnect] will resend the messages.
                      Use [reinitialise](#reinitialise) to reset the client to its
                      original state.
                      Must be set to true if the id parameter is NULL.

  Returns:

     mqtt object (see mqtt_mt) or raises error

  Example:

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
```

[Back to content](#content)

### connect
-----------

  Connect to an MQTT broker.

  Parameters:

    opts.host          - Hostname or IP address of the broker to connect to.
    opts.port          - Network port to connect to. Usually 1883.
    opts.keepalive     - The number of seconds the broker waits since the last 
                         message before sending a PING message to the client.
    opts.log_mask      - LOG_NONE, LOG_INFO, LOG_NOTICE, LOG_WARNING,
                         LOG_ERROR[default], LOG_DEBUG, LOG_ALL.
    opts.auto_reconect - [true|false] - auto reconnect on (default) or off.

  Returns:

    true or false, emsg

  Example:

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
  instance:connect({
    host='127.0.0.1',
    port=1883,
    keepalive=60,
    log_mask=mqtt.LOG_NONE
  })
```

[Back to content](#content)

### reconnect
-------------

  Reconnect to broker.

  This function provides an easy way of reconnecting to the broker after
  connection loss. It uses the values provided in the [connect](#connect) 
  call and must not be called prior.

  Note: After the reconnection you must call [subscribe](#subscribe) to 
  subscribe to topics.

  See the [connect](#connect) `opts.auto_reconect` parameter.

  Example:

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
  instance:connect({host='127.0.0.1', port=1883, auto_reconect=false})
  ok, emsg_or_mid = instance:subscribe('topic')
  if not ok and not mqtt.connect then
    print('subscribe - failed, reconnecting ...')
    ok, _ = instance:reconnect()
  end
```

[Back to content](#content)

### subscribe
-------------

  Subscribe to a topic.

  Parameters:

    sub -  Subscription pattern.
    qos -  Requested Quality of Service for this subscription.

  Returns:

    true or false, integer mid or error message

  Example:

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
  -- Cut, see [connect](#connect)
  ok, err_or_mid = instance:subscribe('my/topic/#', 1)
  if ok then
    print(ok, err_or_mid)
  end
```

[Back to content](#content)

### unsubscribe
---------------

  Unsubscribe from a topic.

  Parameters:

    topic - Unsubscription pattern.

  Returns:

    true or false, integer mid or error message

  Example:

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
  -- Cut, see [connect](#connect)
  ok, err = instance:unsubscribe('my/topic/#')
  if ok then
    print(ok, err)
  end
```

[Back to content](#content)

### destroy
-----------

  Destroy an `mqtt` object.

  Note: Call this function manually as the module does not use the Lua's GC.

  Parameters:

    None

  Returns:

    true or false, error message

  Example:

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
  -- Cut, see [connect](#connect)
  ok, err = instance:destroy()
  if ok then
    print(ok, err)
  end
```

[Back to content](#content)

### publish
-----------

  Publish a message on a given topic.

  Parameters:

    topic      - Null-terminated string of the topic to publish to.
    payload    - Pointer to the data to send.
    qos        - Integer value 0, 1 or 2 indicating the Quality of Service to be
                 used for the message. When you call the library with "mqtt = require('mqtt')",
                 you can use mqtt.QOS_0, mqtt.QOS_1 and mqtt.QOS_2 as a replacement 
                 for some strange digital variable.
    retain     - Set to true to make the message retained. You can also use the values
                 mqtt.RETAIN and mqtt.NON_RETAIN to replace the unmarked variable.

  Returns:

    true or false, emsg, message id (i.e. MID) is referenced in the publish callback

  Example:

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
  -- Cut, see [connect](#connect)
  ok, err = instance:publish('my/topic/#', 'Some payload as string', mqtt.QOS_0, mqtt.RETAIN)
  if ok then
    print(ok, err)
  end
```

[Back to content](#content)

### will_set
------------

  Configure the `will` information for a `mosquitto` instance. By default, clients do
  not have a `will`. Must be called before calling [connect](#connect).

  Parameters:

    topic      - Topic for which to publish the will.
    payload    - Pointer to the data to send.
    qos        - Integer value 0, 1 or 2 indicating the Quality of Service to be
                 used for the will.
    retain     - Set to true to make the will a retained message.

  Returns:

    true or false, emsg

  Example:

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
  -- Cut, see [connect](#connect)
  ok, err = instance:will_set('my/topic/#', 'Some payload as string', 0, true)
  if ok then
    print(ok, err)
  end
```

[Back to content](#content)

### will_clear
--------------

  Remove a previously configured `will`. Must be called before calling
  [connect](#connect).

  Returns:

    true or false, emsg

  Example:

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
  -- Cut, see [connect](#connect)
  ok, err = instance:will_clear()
  if ok then
    print(ok, err)
  end
```

[Back to content](#content)

### login_set
-------------

  Configure a username and password for the `mosquitto` instance. Supported only by 
  the brokers that implement the MQTT spec v3.1. By default, no username 
  or password will be sent. If the username is NULL, the password argument is ignored.

  Must be called before calling [connect](#connect).

  Parameters:

    username - Username to send as a string or NULL to disable
               authentication.
    password - Password to send as a string. Set to NULL to send 
               just a valid username.

  Returns:

    true or false, emsg

  Example:

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
  -- Cut, see [connect](#connect)
  ok, err = instance:login_set('user', 'password')
  if ok then
    print(ok, err)
  end
```

[Back to content](#content)

### tls_insecure_set
--------------------

  Configure server hostname verification in the server certificate. If the 
  `value` parameter is set to `true`, connection encryption is pointless and 
  it is impossible to guarantee that the host you are connecting to is not 
  impersonating your server. This can be useful during the initial server 
  testing but makes it possible for a malicious third party to impersonate 
  your server through, e.g., DNS spoofing.

  Do not use this function in a production environment. 

  Must be called before [connect](#connect).

  Parameters:

    value - If set to false (default), certificate hostname is checked. 
            If set to true, no checking is performed and connection is insecure.

  Returns:

    true or false, emsg

  Example:

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
  -- Cut, see [connect](#connect)
  ok, err = instance:tls_insecure_set(true)
  if ok then
    print(ok, err)
  end
```

[Back to content](#content)

### tls_set
-----------

  Configure a client for certificate-based SSL/TLS support. Must be called
  before [connect](#connect).

  Define certificates signed by a Certificate Authority (CA) as trusted 
  (i.e. the server certificate must be signed by it) using `cafile`.

  If the server to connect to requires clients to provide a certificate, 
  set the `certfile` and `keyfile` paths to your client certificate 
  and private key files. If the private key is encrypted, provide a password 
  callback function or enter the password via the command line.

  Parameters:

    cafile      - Path to a file containing PEM-encoded trusted CA
                  certificate. Either the cafile or capath must not be NULL.
    capath      - Path to a directory containing the PEM-encoded trusted CA
                  certificate files. See mosquitto.conf for more details on
                  configuring this directory. Either the cafile or capath must 
                  not be NULL.
    certfile    - Path to a file containing a PEM-encoded certificate
                  for this client. If NULL, the keyfile must also be NULL and no
                  client certificate will be used.
    keyfile     - Path to a file containing a PEM-encoded private key for
                  this client. If NULL, the certfile must also be NULL and no
                  client certificate will be used.
    pw_callback - TODO: implement me.

  Returns:

    true or false, emsg

  See also: [tls_insecure_set](#tls_insecure_set).

  Example:

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
  -- Cut, see [connect](#connect)
  ok, err = instance:tls_set('my.pem', '/home/user/pems', 'my.ca', 'my.key')
  if ok then
    print(ok, err)
  end
```

[Back to content](#content)

### on_message

  Set a message callback. Called when a message from the broker 
  is received.

  Parameters:

    F - a callback function with the following form:
        function F(integer_mid, string_topic, string_payload, integer_gos, integer_retain)

  Returns:

    true or false, emsg

  Example:

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
  -- Cut, see [connect](#connect)
  ok, err = instance:on_message(
    function(message_id, topic, payload, gos, retain)
      print('Recv. new message',
        message_id, topic, payload, gos, retain)
    end)
  if ok then
    print(ok, err)
  end
```

[Back to content](#content)

### Subscribe to events
-----------------------

  Warning: Use the following functions with caution as 
  incorrect calls can break asynchronous I/O loops!

  Non-mandatory functions:

  * on_connect

  * on_disconnect

  * on_publish

  * on_subscribe

  * on_unsubscribe

  See the detailed documentation of these functions in the [mqqt.init.lua](../mqqt/init.lua) file.

[Back to content](#content)

## Performance tuning
---------------------

TODO: describe me.

[Back to content](#content)

## Examples
-----------

  The [examples/connect.lua](../examples/connect.lua) file shows how to connect 
  to an MQTT broker.

  The [examples/producer_consumer_queue.lua](../examples/producer_consumer_queue.lua) file shows how 
  Tarantool produces, passes, and consumes data to and from an MQTT broker 
  via the MQTT connector in a non-blocking way.

[Back to content](#content)

## Copyright & License
----------------------
[LICENSE](https://github.com/tarantool/mqtt/blob/master/LICENSE)

[Back to content](#content)

## See also
----------
* [Tarantool](http://tarantool.org) homepage.
* MQTT brokers:
  * [Mosquitto](https://mosquitto.org) homepage.
  * [RabbitMQ](https://www.rabbitmq.com) homepage.

[Back to content](#content)

---

Please report bugs at https://github.com/tarantool/mqtt/issues.

We also warmly welcome your feedback in the discussion mailing list: tarantool@googlegroups.com.
