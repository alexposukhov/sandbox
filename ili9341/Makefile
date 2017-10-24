CONFIG_MODULE_SIG=n

obj-m := ili9341_fb.o

#KDIR ?= ${HOME}/BBB/bb_kernel/
KDIR ?= ${BUILD_KERNEL}

#PWD := $(shell pwd)
PWD := $(CURDIR)

default:
		  $(MAKE) ARCH=arm  -C $(KDIR) M=$(PWD) modules

clean:
		  $(MAKE) ARCH=arm  -C $(KDIR) SUBDIRS=$(PWD) clean

config:
		$(MAKE) ARCH=arm -C $(KDIR) menuconfig

dtb:
		$(MAKE) ARCH=arm -C $(KDIR) dtbs

.PHONY: clean default config dtb
