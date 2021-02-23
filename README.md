# Kiss-Light

Yet another home automation hub, where it can be essentially hosted on anything, and the client is a simple command line application.

The idea is essentially make it to make the implementation as simple as possible, but aim for expert home automation enthusiasts.

The end goal is to make this portable enough to work on a standard Rasbperry Pi, but possible to throw in an openwrt-enabled
router as an enclosed solution.

## Installation

make sure the following is installed prior:

- sqlite3
- mosquitto

To install the prerequisites, the following command will do the trick:

(for Ubuntu/Debian systems)

```shell
sudo apt install sqlite3 libsqlite3-dev mosquitto mosquitto-clients
```

after that, run the following:

```shell
make
sudo make install
```

And the program should be up and running after the sudo make install step.

## Supported Devices

### WORK IN PROGRESS

Anything that supports mqtt, though [Tasmota](https://tasmota.github.io/docs/) was used in developing this program.

Currently supported types of devices (as of v0.2 and later):

- Outlets/toggleable devices (like [this](https://templates.blakadder.com/gosund_wp3.html)) (use as dev_type 0)

- Power strip (like [this](https://templates.blakadder.com/gosund_WP9.html)) (use as dev_type 1)

- Dimmable light bulbs (like [this](https://templates.blakadder.com/merkury_MI-BW320-999W.html)) (use as dev_type 2) (***UNTESTED!***)

- CCT light bulbs (like [this](https://templates.blakadder.com/mimoodz_A21.html)) (use as dev_type 3) (***UNTESTED!***)

- RGB light bulb (like [this](https://templates.blakadder.com/moko_YX-L01C-E27.html)) (use as dev_type 4) (***UNTESTED!***)

- RGBW light bulb (like [this](https://templates.blakadder.com/merkury_MI-BW210-999W_QPW92.html)) (use as dev_type 5)

- RGB CCT light bulb (like [this](https://templates.blakadder.com/aigital-LE13.html)) (use as dev_type 6) (***UNTESTED!***)

- Custom Mqtt devices (WORK IN PROGRESS) (use as dev_type 7)

## Kiss-Light Return Codes

The kiss-light protocol takes some inspiration from the HTTP protocol, but
has fewer and more specific codes.

```plaintext
200 series success codes:

200 -- device toggled

201 -- device state set

202 -- device added

203 -- device deleted

204 -- list of devices

205 -- custom command sent

206 -- current device state

207 -- client exit
__________________________________________
400 series error codes:

400 -- bad request, did not understand what was sent

401 -- device's state is unknown (state not initialized usually)

402 -- unable to remove device (doesn't exist generally)

403 -- unable to add device (maximum devices met)

404 -- no such device

405 -- incorrect input (when explicitely setting device state)

406 -- unable to detect kisslight version (will become important as time goes on)

407 -- not yet implemented

408 -- device already exists (when trying to add a duplicate device)
__________________________________________
500 series error codes:

500 -- error on the mqtt side of things

505 -- too many simultaneous connections at once, try again later
```

## Client

The respository comes with a simple but effective command line client written in golang.

The usage currently looks like this:

```shell
computer ~ $ kl-client
To change states of existing devices:

usage: kl-client set <device name> on|off
                   toggle <device name>
                   send <topic> <command>

Adding/deleting, status, and listing of devices:

usage: kl-client add <device name> <mqtt_topic> <dev_type>
                   delete <device name>
                   status <device name>
                   list

Updating server IP address, or port:

usage: kl-client ip <IP address>
                   port <port number>

```

### Add/Delete, list, and get device status

Add a device:

```shell
computer ~ $ kl-client add outlet0 topic 0
added Device outlet0 successfully
```

Delete a device:

```shell
computer ~ $ kl-client delete outlet0
deleted device outlet0 successfully
```

List devices:

```shell
computer ~ % kl-client list
here is the list of 1 devices:
(device name -- mqtt topic -- device type)
outlet0 -- topic -- outlet/toggleable
```

Get a device's status:

```shell
computer ~ $ kl-client status outlet0
device outlet0 state is currently OFF
```

### Change device's power states

Set device on or off:

```shell
computer ~ $ kl-client set outlet0 on
successfully set device outlet0 on
computer ~ $ kl-client set outlet0 off
successfully set device outlet0 off
```

Toggle device:

```shell
computer ~ $ kl-client toggle outlet0
toggled device outlet0 successfully
```

send custom topic and command:

```shell
computer ~ $ kl-client send cmnd/tasmota/POWER OFF
transmitted cmnd/tasmota/POWER OFF successfully
```

### Updating client's IP address or port number

Update ip address of server: (say server IP has changed, client does not yet know that)

```shell
computer ~ $ kl-client ip 127.0.0.1
IP address 127.0.0.1 set
```

Updating port of server: (because port ```1555``` was already used for some reason or other)

```shell
computer ~ $ kl-client port 2423
port 2423 set
```

## Using Telnet

### WORK IN PROGRESS, Things are still subject to change

It should be noted that numbers here are in decimal, unless otherwise specified.

It is assumed that mosquitto has been setup prior, for now without TLS but TLS may eventually get implemented. and it will be assumed that
port ```1883``` is used for mosquitto.

It is also assumed that a device in use is sporting the tasmota or some other open source firmware to allow the use of mqtt, and that the device
itself is already connected to the appropriate network and knows information relating to the mosquitto server.

Finally, the default port for this server is ```1155```, so make sure to use that port, or whatever is set in the configuration for this program when using telnet.

### Changing Device States

Transmit a custom mqtt topic and command without storing it into a database:

(though it will update the state of a device if sending a command to the topic of a stored device)

```plaintext
Template:
TRANSMIT <full topic> <command> KL/<version#>
KL/<version#> 200 custom command <full topic> <command> sent

Example in practice:
TRANSMIT cmnd/tasmota/power toggle KL/0.3
KL/0.3 200 custom command cmnd/tasmota/power toggle sent
```

Toggling the saved device can be done as follows:

```plaintext
Template:
TOGGLE <device name> KL/<version#>
KL/<version#> 200 Device <device name> toggled

Example in practice:
TOGGLE outlet KL/0.3
KL/0.3 200 device outlet toggled
```

Explicitely changing the device state:

```plaintext
Template:
SET <device name> <command> <command arg> KL/<version#>
KL/<version#> 200 Device <device name> <On or Off>

Example in Practice:
SET outlet ON KL/0.3
KL/0.3 200 device outlet ON

or

SET outlet OFF KL/0.3
KL/0.3 200 device outlet OFF
```

### Adding/Deleting Devices

Adding a device:

***device type is not yet fully fleshed-out, it is assumed that the device in question is an outlet.

```plaintext
Template:
ADD <device name> <topic> <device type> KL/<version#>
KL/<version#> 200 device <device name> added

Example in practice:
ADD outlet tasmota 0 KL/0.3
KL/0.3 200 device outlet added
```

Deleting a device:

```plaintext
Template:
DELETE <device name> KL/<version#>
KL/<version#> 200 device <device name> deleted

Example in Practice:
DELETE outlet KL/0.2
KL/0.3 200 device outlet deleted
```

### Status of Device(s)

List devices stored on server:

```plaintext
Template:
LIST KL/<version#>
KL/<version#> 200 number of devices: n
(n line of device names, respective topic, and dev_type as a string)

Example in Practice:
LIST KL/0.3
KL/0.3 200 number of devices: 5
outlet -- tasmota -- outlet/toggleable
strip -- strip0 -- powerstrip
bulb -- rgbbulb0 -- rgbbulb
lamp -- light0 -- dimmablebulb
prototype -- prototype1 -- unkown/other
```

Retreiving a device's current state:

```plaintext
Template:
STATUS <device name> KL/<version#>
KL/<version#> 200 device <device name> is <status>

Example in Practice:
STATUS outlet KL/0.3
KL/0.3 200 device outlet is OFF
```

### to Quit

```plaintext
Q
KL/0.2> 200 goodbye
Connection closed by foreign host.
computer ~ $
```

****version#** is currently ```0.3```*

## How server works

### initialization

Within the src directory, the file ```main.c``` is where the main function is located.

Main returns a 1 if something fails to initialize, or a 0 if it exits cleanly.

Here is what main() does in a nutshell:

1. Initialize logger.
2. Initialize configuration parser and migrate information from ini file to configuration struct.
3. Analyze command line args, override ini file if desired. Establish signal_handler() (located in ```daemon.c```) for SIGINT as well. (Not Yet Implemented, only checks if user wants to run program as a daemon)
4. Allocate Buffers for server, mqtt functions, sqlite functions, via allocate_buffers() function above the main function.
5. Initialize mutex semaphores, both a pthread_mutex_t and a sem_t semaphores.
6. Initialize sqlite functions, migrate information from database to memory as a struct array.
7. Initialize MQTT client.
8. Create MQTT client and database updater threads.
9. Finally, the kiss-light server itself is initialized, entering the server_loop() in ```server.c```.

### connection retreived

1. Before the loop is run in the function server_loop within ```server.c```, values have to be initialized.
2. Once inside the loop, wait for a file descriptor to be made ready in 50 milliseconds.
3. The 0th client (the server itself) will then accept a new connection.
4. Upon a new connection request, the server_connection_handler() function would take over to handle any later requests made by the client.

### request retreived

1. Within the server_connection_handler() function in ```server.c```, the server waits for a request with the read function.
2. When a request is retreived, the function parse_server_request() is then called.
3. In the parse_server_request() function, scan all possible args from the getgo, and memset the buf for response use.
4. After analyzing the args, buf will then have been updated with the proper response, and returned a code to server_connection_handler().
5. The connection handler sends the response back to the client, and repeat.

### mqtt client thread

The thread function client_refresher() simply just refreshes itself via the mqtt_sync() function within ```mqtt.c``` (which isn't modified by me in any way) in a forever loop which sleeps for about 100 milliseconds.

### database updater thread

This thread function db_updater() initially sleeps for 5 seconds, then in the forever loop, it analyzes the to_change[] int array to handle any updates that may have to updated. After which will sleep for another 5 seconds, and repeat.

The values in the to_change[] array correspond to the following:

```plaintext
-1 -- No change
0 -- Update dev_state (most likely to be changed frequently)
1 -- Update mqtt_name
2 -- Update mqtt_topic
3 -- Update all of an entry (simplifies things greatly)
4 -- Add new device to database
5 -- Remove device from database
```

### upon exit

1. When hit with a SIGINT request (or Ctrl+C), handle_signal will call close_socket() in ```server.c```, which sets the global variable closeSocket in ```server.c``` to 1.
2. When server_loop() checks if closeSocket is greater than 0, it is true and will break out of the loop.
3. The main() in ```main.c``` will then cancel the mqtt client and database updater threads and have the resources join back to the main process.
4. finally main() run the cleanup() command directly above the main function, and main() will return 0 upon exit.

### other details

- Server currently supports up to 10 simultaneous connections, but this can be changed in ```server.h``` if desired.
- Server can support 50 devices by default, but ```/etc/kisslight.ini``` can be changed to support more, or less depending.
- Server database location is ```/var/lib/kisslight/kisslight.db``` but can also be updated in the ```/etc/kisslight.ini``` file.
- Log is located in ```/var/log/kisslight/kisslight.log```.

## Credits

[HamletXiaoyu](https://github.com/HamletXiaoyu) for socket poll demo. [[repo](https://github.com/HamletXiaoyu/socket-poll)]

[jirihnidek](https://github.com/jirihnidek) for example daemon [[repo](https://github.com/jirihnidek/daemon)]

[benhoyt](https://github.com/benhoyt) for ini parser [[repo](https://github.com/benhoyt/inih)]

[LiamBindle](https://github.com/LiamBindle) for mqtt-c library [[repo](https://github.com/LiamBindle/MQTT-C)]

[rxi](https://github.com/rxi) for the log library [[repo](https://github.com/rxi/log.c)]

[zserge](https://github.com/zserge) for the jsmn library [[repo](https://github.com/zserge/jsmn)]

others I may have missed...
