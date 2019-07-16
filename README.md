# Kiss-Light

Yet another RF outlet controller, where it is controlled from a Raspberry Pi, and the client is a simple command line application.

The idea is essentially make it as simple as possible, but still have useful functionality that could easily be used in home automation or just used standalone for DIYers.

It should be noted that this project is used in conjunction with 433MHz RF modules, it could probably work with 315MHz RF Modules, but that has not been tested currently.

let's dive right into it.

## Wiring

![RPi wiring](./RPI_RF_433_wiring_diagram.png)

Wire it up exactly as shown, and it should work.

## Installation (server)

make sure the following is installed (on a Raspberry Pi or compatible SBC) prior:

- wiringPi
- sqlite3

then, just run the following:

```shell
make
sudo make install
```

And the program should be up and running after the sudo make install step.

## Installation (client)

Make sure GoLang is installed, and is at least version 1.6 or later.

For now, this requires you to know what the specific codes are to control your RF outlet,
the pulse it is okay with, and what IP addresss the Raspberry Pi server has.

Once the code over in ```src/client/client.go``` has the mentioned info, the client can be built as follows:

```shell
make client
./client
```

This makes it so there is less to type, but the device can alternatively be controlled via telnet.

## Using Telnet and How Server Works

It should be noted that numbers here are in decimal, unless otherwise specified.

The default port for this server is ```1155```, so make sure to use that port, or whatever is set in the configuration for this program when using telnet.

Once connected, the following will transmit the given RF code with the given pulse:

```plaintext
Template:
TRANSMIT <code> <pulse> KL/<version#>
KL/<version#> 200 Custom Code Sent

Example in practice:
TRANSMIT 5592371 189 KL/0.1
KL/0.1 200 Custom Code Sent
```

A code and its pulse is acquirable from an RF Remote (for example) that may control
another desired outlet:

```plaintext
Template:
SNIFF KL/<version#>
KL/<version#> 200 Sniffing
<enter desired button from RF remote>
KL/<version#> 200 Code: <code> Pulse: <pulse>

Example in Practice:
SNIFF KL/0.1
KL/0.1 200 Sniffing
<enter desired button from RF remote>
KL/0.1 200 Code: 5592380 Pulse: 188
```

Once On and Off codes have been recorded somewhere, it is possible to save
those to the server's database:

```plaintext
Template:
ADD <device name> <on_code> <off_code> <pulse> KL/<version#>
KL/<version#> 200 Device <device name> Added

Example in practice:
ADD lamp 5592371 5592380 189 KL/0.1
KL/0.1 200 Device lamp Added
```

Toggling the saved device can be done as follows:

```plaintext
Template:
TOGGLE <device name> KL/<version#>
KL/<version#> 200 Device <device name> Toggled

Example in practice:
TOGGLE lamp KL/0.1
KL/0.1 200 Device lamp Toggled
```

Suppose there are several devices added, and it is desired
that a client knows what devices exist on the server, here
is how it can be done:

```plaintext
Template:
LIST KL/<version#>
KL/<version#> 200 Number of Devices n
(n line of device names, up to 30 currently)

Example in Practice:
LIST KL/0.1
KL/0.1 200 Number of Devices 1
lamp
```

Deleting a device can also be done as follows:

```plaintext
Template:
DELETE <device name> KL/0.1
KL/0.1 200 Device <device name> Deleted

Example in Practice:
DELETE lamp KL/0.1
KL/0.1 200 Device lamp Deleted
```

Finally, exiting from the server fairly cleanly is also doable:

```plaintext
Q KL/<version#>
KL/<version#> 200 Goodbye
Connection closed by foreign host.
computer ~ $
```

****version#** is currently ```0.1```*

## Credits

[HamletXiaoyu](https://github.com/HamletXiaoyu) for socket poll demo. [[repo](https://github.com/HamletXiaoyu/socket-poll)]

[timleland](https://github.com/timleland) for the RCSwitch code [[repo](https://github.com/timleland/rfoutlet)] [[blog](https://timleland.com/wireless-power-outlets/)]

[jirihnidek](https://github.com/jirihnidek) for example daemon [[repo](https://github.com/jirihnidek/daemon)]

[benhoyt](https://github.com/benhoyt) for ini parser [[repo](https://github.com/benhoyt/inih)]

others I may have missed...
