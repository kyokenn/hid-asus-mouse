obj-m+=hid-asus-mouse.o
KERNELDIR?=/lib/modules/$(shell uname -r)/build
DRIVERDIR?=$(shell pwd)

VERSION=0.2.0
SRC=\
	Makefile \
	README.md \
	dkms.conf \
	hid-asusmouse-kmod.spec \
	hid-asusmouse-kmod-common.spec \
	hid-asus-mouse.c \
	hid-asus-mouse.h
SRCDIR=hid-asusmouse_$(VERSION)
ARCHIVE=$(SRCDIR).orig.tar.xz

all:
	echo "No need to build kernel modules now."
	echo "Kernel modules are built at packages installation time using DKMS or AKMOD."

# invoked by debian/rules
install:
	mkdir -p $(DESTDIR)/usr/src/hid-asusmouse-$(VERSION)
	cp -fv hid-asus-mouse.c hid-asus-mouse.h Makefile $(DESTDIR)/usr/src/hid-asusmouse-$(VERSION)/

# invoked by dkms
kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(DRIVERDIR) modules

kernel_modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(DRIVERDIR) modules_install

kernel_clean:
	$(MAKE) -C $(KERNELDIR) M=$(DRIVERDIR) clean

# build source archive, needed by rpm and deb
../$(ARCHIVE): $(SRC)
	mkdir $(SRCDIR)
	cp -fv $^ $(SRCDIR)/
	tar Jcvf $@ $(SRCDIR)
	rm -Rf $(SRCDIR)

# copies specs to rpmbuild's specs path
~/rpmbuild/SPECS/%.spec: %.spec
	cp -fv $^ $@

# copies archive from parent directory to rpmbuild's sources path
~/rpmbuild/SOURCES/%.tar.xz: ../%.tar.xz
	cp -fv $^ $@

rpm: \
	~/rpmbuild/SOURCES/$(ARCHIVE) \
	~/rpmbuild/SPECS/hid-asusmouse-kmod.spec \
	~/rpmbuild/SPECS/hid-asusmouse-kmod-common.spec
	rpmbuild -bb hid-asusmouse-kmod.spec
	rpmbuild -bb hid-asusmouse-kmod-common.spec

deb: ../$(ARCHIVE)
	debuild -uc -us
