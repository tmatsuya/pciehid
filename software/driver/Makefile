ifneq ($(KERNELRELEASE),)
obj-m	 	:= pciehid.o
#obj-m		:= tcam.o
#tcam-y	 	:= pciehid_main.o
else
KDIR		:= /lib/modules/$(shell uname -r)/build/
PWD		:= $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules


clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	install -m 644 $(PWD)/*.ko /lib/modules/`uname -r`/kernel/drivers/misc
	if [ -d /etc/udev/rules.d -a ! -f /etc/udev/rules.d/99-pciehid.rules ] ; then \
		install -m 644 99-pciehid.rules /etc/udev/rules.d ; \
	fi
	depmod -a
endif
