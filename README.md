hid-asus-mouse
==============

HID driver for ASUS ROG & TUF mice
providing generation of keyboard events.


Device compatibility
--------------------

Device name                   | Generation | HID descriptor | Keyboard events
------------------------------|------------|----------------|----------------
**Gladius**                   | 1          | BROKEN         | needs [**gen1** driver](gen1)
**Spatha**                    | 1          | BROKEN         | needs [**gen1** driver](gen1)
**Strix Evolve**              | 1          | BROKEN         | needs [**gen1** driver](gen1)
**Buzzard**                   | 1          | OK             | needs [**gen1** driver](gen1)
**Gladius II Origin PNK LTD** | 1          | OK             | needs [**gen1** driver](gen1)
**Gladius II Origin**         | 1          | OK             | needs [**gen1** driver](gen1)
**Gladius II**                | 1          | OK             | needs [**gen1** driver](gen1)
**Pugio**                     | 1          | OK             | needs [**gen1** driver](gen1)
**Strix Carry**               | 1          | OK             | needs [**gen1** driver](gen1)
**Strix Impact**              | 1          | OK             | needs [**gen1** driver](gen1)
**Strix Impact II Wireless**  | 1          | OK             | needs [**gen1** driver](gen1)
**Chakram**                   | 2          | OK             | no driver needed
**Keris Wireless**            | 2          | OK             | no driver needed

Devices with broken HID descriptors needs some fixes using kernel module, so patches are welcome.

You can’t change the mouse settings without descriptor fixes but keyboard events should work.


Installing driver for generation 1 devices
------------------------------------------

Building the **gen1** driver

```
make -C gen1
```

Installing the **gen1** driver

```
make -C gen1 install
```

Installing driver for generation 2 devices
------------------------------------------

**WARNING!!!**

You don’t have to do this if you have generation 2 device!
Keyboard events should work with generic vanilla kernel.
This driver is made as reference for testing and debugging purposes.

Building the **gen2** driver

```
make -C gen2
```

Installing the **gen2** driver

```
make -C gen2 install
```
