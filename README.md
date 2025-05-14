hid-asus-mouse
==============

**hid-asus-mouse** is a custom HID driver for ASUS ROG & TUF mice.
It is a kernel module with a custom HID logic. It creates a virtual input device,
which combines all the mouse and keyboard interfaces and used for event generation.
The input device is shown at "/proc/bus/input/devices" as "ASUS mouse input".


Features
--------

* Single virtual input device for mouse and keyboard event generation,
so they doesn't conflict with each other.
* Handles keyboard events.
* Smooth wheel emulation using keypad keys: KEY_KP2, KEY_KP4, KEY_KP6, KEY_KP8.


Supported devices
-----------------

[Device IDs](hid-asus-mouse.h)


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
