KERNEL_DIR ?= ../../linux-imx-4.1.15-2.1.0-e48931b1-v2.8
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

obj-m += p1_led.o
obj-m += p1_beep.o
obj-m += p1_key.o
obj-m += p1_ap3216c.o
obj-m += p1_icm20608.o

.PHONY: all clean

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) clean

