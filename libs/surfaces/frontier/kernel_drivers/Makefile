ifneq ($(KERNELRELEASE),)

obj-m := tranzport.o
tranzport-objs := 

else

KDIR   := /lib/modules/$(shell uname -r)/build
PWD    := $(shell pwd)
MODDIR := $(DESTDIR)/lib/modules/$(shell uname -r)/kernel/sound/usb/misc
BINDIR := $(DESTDIR)/usr/local/bin

default::
	$(MAKE) -Wall -C $(KDIR) SUBDIRS=$(PWD) modules
	$(MAKE) -C tests

install-only:: default
	mkdir -p $(MODDIR) $(BINDIR)
	cp tranzport.ko $(MODDIR)
	$(MAKE) -C tests install

install:: install-only
	/sbin/depmod -a
	+/sbin/rmmod tranzport
	/sbin/modprobe tranzport

irt:: install
	tranzport_tests.sh

clean::
	rm -f core .*.cmd *.o *.ko *.mod.c Module.symvers *.bak .\#* *~
	rm -rf .tmp_versions
	$(MAKE) -C tests clean

endif
