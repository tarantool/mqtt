<a href="https://travis-ci.org/tarantool/mqtt">
  <img src="https://travis-ci.org/tarantool/mqtt.png?branch=master">
</a>

# Tarantool MQTT client
---------------------------------
Key features
* allows to create communication with MQTT broker in non-blocking way
* TLS supported
* pretty low overheads, code based on libmosquitto

## Content
----------
* [Compilation and install](#compilation-and-install)
* [API](#api)
  * [new](#new)
  * [connect](#connect)
  * [reconnect](#reconnect)
  * [subscribe](#subscribe)
  * [unsubscribe](#unsubscribe)
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

## Compilation and install
--------------------------
  Before reading any further, make sure, you have installed MQTT broker

### Build from source

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

### new
-------

  Create a new mosquitto client instance.

  Parameters:
  	id -            String to use as the client id. If NULL, a random client id
  	                will be generated. If id is NULL, clean_session must be true.
  	clean_session - set to true to instruct the broker to clean all messages
                   and subscriptions on disconnect, false to instruct it to
                   keep them. See the man page mqtt(7) for more details.
                   Note that a client will never discard its own outgoing
                   messages on disconnect. Calling (connect)[#connect] or
                   (reconnect)[#reconnect] will cause the messages to be resent.
                   Use [reinitialise](#reinitialise) to reset a client to its
                   original state.
                   Must be set to true if the id parameter is NULL.

  Returns:
     mqtt object(see mqtt_mt) or raise error

```lua
  mqtt = require('mqtt')
  instance = mqtt.new("client_id", true)
```

[Back to content](#content)

### connect
-----------

  Connect to an MQTT broker.

  Parameters:
    opts.host          - the hostname or ip address of the broker to connect to.
    opts.port          - the network port to connect to. Usually 1883.
    opts.keepalive     - the number of seconds after which the broker should send a PING
                         message to the client if no other messages have been exchanged
                         in that time.
    opts.log_mask      - LOG_NONE, LOG_INFO, LOG_NOTICE, LOG_WARNING,
                         LOG_ERROR[default], LOG_DEBUG, LOG_ALL
    opts.auto_reconect - true[default], false - auto reconnect on, off

  Returns:
    true or false, emsg

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

  Reconnect to a broker.

    This function provides an easy way of reconnecting to a broker after a
    connection has been lost. It uses the values that were provided in the
    [connect](#connect) call. It must not be called before
    [connect](#connect).

  NOTE
    After reconnecting you must call [subscribe](#subscribe) for subscribing
    to a topics.

  See
    [connect](#connect) <opts.auto_reconect>

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
    sub -  the subscription pattern.
    qos -  the requested Quality of Service for this subscription.

  Returns:
    true or false, integer mid or error message

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
    topic - the unsubscription pattern.

  Returns:
    true or false, integer mid or error message

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

### publish
-----------

  Publish a message on a given topic.

  Parameters:
    topic -      null terminated string of the topic to publish to.
 	  payload -    pointer to the data to send.
 	  qos -        integer value 0, 1 or 2 indicating the Quality of Service to be
                 used for the message.When you call the library as "mqtt = require('mqtt')"
                 can be used mqtt.QOS_0, mqtt.QOS_1 and mqtt.QOS_2 as a replacement for some strange
                 digital variable
 	  retain -     set to true to make the message retained. You can also use the values
                 mqtt.RETAIN and mqtt.NON_RETAIN to replace the unmarked variable

  Returns:
    true or false, emsg, message id(e.g. MID) is referenced in the publish callback

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

  Configure will information for a mosquitto instance. By default, clients do
  not have a will.  This must be called before calling [connect](#connect).

  Parameters:
  	topic -      the topic on which to publish the will.
  	payload -    pointer to the data to send.
  	qos -        integer value 0, 1 or 2 indicating the Quality of Service to be
                used for the will.
  	retain -     set to true to make the will a retained message.

  Returns:
   true or false, emsg

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

  Remove a previously configured will. This must be called before calling
  [connect](#connect).

  Returns:
    true or false, emsg

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

  Configure username and password for a mosquitton instance. This is only
  supported by brokers that implement the MQTT spec v3.1. By default, no
  username or password will be sent.
  If username is NULL, the password argument is ignored.
  This must be called before calling connect.

  This is must be called before calling [connect](#connect).

  Parameters:
  	username - the username to send as a string, or NULL to disable
              authentication.
  	password - the password to send as a string. Set to NULL when username is
  	           valid in order to send just a username.

  Returns:
   true or false, emsg


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

  Configure verification of the server hostname in the server certificate. If
  value is set to true, it is impossible to guarantee that the host you are
  connecting to is not impersonating your server. This can be useful in
  initial server testing, but makes it possible for a malicious third party to
  impersonate your server through DNS spoofing, for example.
  Do not use this function in a real system. Setting value to true makes the
  connection encryption pointless.
  Must be called before [connect](#connect).

  Parameters:
   value - if set to false, the default, certificate hostname checking is
           performed. If set to true, no hostname checking is performed and
           the connection is insecure.

  Returns:
   true or false, emsg


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

  Configure the client for certificate based SSL/TLS support. Must be called
  before [connect](#connect).

  Define the Certificate Authority certificates to be trusted (ie. the server
  certificate must be signed with one of these certificates) using cafile.

  If the server you are connecting to requires clients to provide a
  certificate, define certfile and keyfile with your client certificate and
  private key. If your private key is encrypted, provide a password callback
  function or you will have to enter the password at the command line.

  Parameters:
   cafile -      path to a file containing the PEM encoded trusted CA
                 certificate files. Either cafile or capath must not be NULL.
   capath -      path to a directory containing the PEM encoded trusted CA
                 certificate files. See mosquitto.conf for more details on
                 configuring this directory. Either cafile or capath must not
                 be NULL.
   certfile -    path to a file containing the PEM encoded certificate file
                 for this client. If NULL, keyfile must also be NULL and no
                 client certificate will be used.
   keyfile -     path to a file containing the PEM encoded private key for
                 this client. If NULL, certfile must also be NULL and no
                 client certificate will be used.
   pw_callback - TODO Implement me

  Returns:
   true or false, emsg

  See Also:
   [tls_insecure_set](#tls_insecure_set)

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

  Set the message callback. This is called when a message is received
  from the broker.

  Parameters:
   F - a callback function in the following form:
       function F(integer_mid, string_topic, string_payload, integer_gos, integer_retain)

  Returns:
   true or false, emsg

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

### Subscribe on events
----------------------
NOTE
    Basically, you don't need those functions, so use them with caution.
    Incorrect uses of following functions might break async I/O loop!

    * on_connect

    * on_disconnect

    * on_publish

    * on_subscribe

    * on_unsubscribe

    * on_disconnect

    You can find Detailed documentation of those functions at file: mqtt/init.lua

[Back to content](#content)

## Performance tuning
---------------------

[Back to content](#content)

## Examples
-----------

  Connecting to the MQTT broker
    * examples/connect.lua

  Tarantool produces data then passes it to MQTT broker via MQTT connector
  in non-blocking way.
  Also, Tarantool consumes data from the MQTT broker via MQTT connector
  in non-blocking way.
    * examples/producer_consumer_queue.lua

[Back to content](#content)

## Copyright & License
----------------------
[LICENSE](https://github.com/tarantool/mqtt/blob/master/LICENSE)

[Back to content](#content)

##See also
----------
* [Tarantool](http://tarantool.org) homepage.
* MQTT brokers
  * [Mosquitto](https://mosquitto.org) homepage.
  * [RabbitMQ](https://www.rabbitmq.com) homepage.

[Back to content](#content)

================
Please report bugs at https://github.com/tarantool/mqtt/issues.

We also warmly welcome your feedback in the discussion mailing list, tarantool@googlegroups.com.
