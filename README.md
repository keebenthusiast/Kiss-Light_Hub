# Kiss-Light

### WORK IN PROGRESS

Yet another home automation hub, where it can be essentially hosted on anything, and the client is a simple command line application.

The idea is essentially make it to make the implementation as simple as possible, but aim for expert home automation enthusiasts.

## Installation

### WORK IN PROGRESS

make sure the following is installed prior:

- sqlite3
- mosquitto
- nginx

---

To install the prerequisites, the following command will do the trick:

(for Ubuntu/Debian systems)

```shell
sudo apt install sqlite3 libsqlite3-dev mosquitto mosquitto-clients nginx
```

after that, run the following:

```shell
make
sudo make install
```

And the program should be up and running after the sudo make install step.

## Using Telnet and How Server Works

### WORK IN PROGRESS

It should be noted that numbers here are in decimal, unless otherwise specified.

The default port for this server is ```1155```, so make sure to use that port, or whatever is set in the configuration for this program when using telnet.

Once connected, the following will transmit the given RF code with the given pulse:

```plaintext
Template:
TRANSMIT <code> <pulse> KL/<version#>
KL/<version#> 200 Custom Code Sent

Example in practice:
TRANSMIT 5592371 189 KL/0.2
KL/0.2 200 Custom Code Sent
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
SNIFF KL/0.2
KL/0.2 200 Sniffing
<enter desired button from RF remote>
KL/0.2 200 Code: 5592380 Pulse: 188
```

Once On and Off codes have been recorded somewhere, it is possible to save
those to the server's database. As of version `0.2` however, it is now
possible to only need the On code or Off code to save it, as demonstrated:

```plaintext
Template:
ADD <device name> <On or Off code> <pulse> KL/<version#>
KL/<version#> 200 Device <device name> Added

Example in practice:
ADD lamp 5592371 189 KL/0.2
KL/0.2 200 Device lamp Added
```

Toggling the saved device can be done as follows:

```plaintext
Template:
TOGGLE <device name> KL/<version#>
KL/<version#> 200 Device <device name> Toggled

Example in practice:
TOGGLE lamp KL/0.2
KL/0.2 200 Device lamp Toggled
```

Explicitely setting the saved device on or off is now doable
as of KL version `0.2`:

```plaintext
Template:
SET <device name> <ON or OFF> KL/<version#>
KL/<version#> 200 Device <device name> <On or Off>

Example in Practice:
SET lamp ON KL/0.2
KL/0.2 200 Device lamp On

or

SET lamp OFF KL/0.2
KL/0.2 200 Device lamp Off
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
LIST KL/0.2
KL/0.2 200 Number of Devices 1
lamp
```

Deleting a device can also be done as follows:

```plaintext
Template:
DELETE <device name> KL/<version#>
KL/0.2 200 Device <device name> Deleted

Example in Practice:
DELETE lamp KL/0.2
KL/0.2 200 Device lamp Deleted
```

Finally, exiting from the server fairly cleanly is also doable:

```plaintext
Q
KL/0.2> 200 Goodbye
Connection closed by foreign host.
computer ~ $
```

****version#** is currently ```0.2```*

## Credits

### WORK IN PROGRESS

[HamletXiaoyu](https://github.com/HamletXiaoyu) for socket poll demo. [[repo](https://github.com/HamletXiaoyu/socket-poll)]

[jirihnidek](https://github.com/jirihnidek) for example daemon [[repo](https://github.com/jirihnidek/daemon)]

[benhoyt](https://github.com/benhoyt) for ini parser [[repo](https://github.com/benhoyt/inih)]

others I may have missed...
