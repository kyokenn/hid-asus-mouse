hid-asus-mouse
==============

HID driver for ASUS ROG & TUF mice providing generation of keyboard events.


Device compatibility
--------------------

Device name                      | HID descriptor | Keyboard events
---------------------------------|----------------|----------------
**Spatha**                       | BROKEN         | ARRAY
**Strix Evolve**                 | BROKEN         | ARRAY
**Buzzard**                      | OK             | ARRAY
**Gladius II**                   | OK             | ARRAY
**Gladius II Origin**            | OK             | ARRAY
**Gladius II Origin PNK LTD**    | OK             | ARRAY
**Pugio**                        | OK             | ARRAY
**Pugio II**                     | OK             | ARRAY
**Strix Carry**                  | OK             | ARRAY
**Strix Impact**                 | OK             | ARRAY
**Strix Impact II Electro Punk** | OK             | ARRAY
**Strix Impact II Wireless**     | OK             | ARRAY
**Chakram**                      | OK             | BITMASK
**Keris Wireless**               | OK             | BITMASK

Devices with **BROKEN** HID descriptors needs some fixes using kernel module, so patches are welcome.

You can’t change the mouse settings without descriptor fixes but keyboard events should work.

Devices with keyboard events packed as **BITMASK** may not needed this driver
and should work with vanilla kernel. **BITMASK** driver mode was made
as reference implementation for testing and debugging purposes.

All devices connected via bluetooth should also work without this driver.


Building RPM
------------

Install build dependencies:
```
sudo dnf build-dep hid-asusmouse-kmod.spec
```

Build RPM package:
```
make rpm
```


Building DEB
------------

Install build dependencies:
```
sudo apt build-dep .
```

Build DEB package:
```
make deb
```
