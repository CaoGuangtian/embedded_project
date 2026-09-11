KERNEL_DIR ?= ../../linux-imx-4.1.15-2.1.0-e48931b1-v2.8
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

obj-m += dm_beep.o
obj-m += dm_ap3216c_iio.o
obj-m += dm_icm20608_iio.o

.PHONY: all clean

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) clean
