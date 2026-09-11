CC ?= arm-linux-gnueabihf-gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -O2
LDLIBS ?= -pthread
CPPFLAGS += -Iinclude

TOOLS := \
	tools/dm_outctl \
	tools/dm_keyread \
	tools/dm_ap3216c_read \
	tools/dm_icm20608_read \
	collector/dm_collector

.PHONY: all clean

all: $(TOOLS)

tools/dm_outctl: tools/dm_outctl.c include/dm_protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

tools/dm_keyread: tools/dm_keyread.c include/dm_protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

tools/dm_ap3216c_read: tools/dm_ap3216c_read.c include/dm_protocol.h include/dm_iio.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

tools/dm_icm20608_read: tools/dm_icm20608_read.c include/dm_protocol.h include/dm_iio.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

collector/dm_collector: collector/dm_collector.c include/dm_protocol.h include/dm_iio.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) $(TOOLS)
