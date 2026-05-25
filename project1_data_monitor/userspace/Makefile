CC ?= arm-linux-gnueabihf-gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -O2
LDLIBS ?= -pthread
CPPFLAGS += -Iinclude

TOOLS := \
	tools/p1_outctl \
	tools/p1_keyread \
	tools/p1_ap3216c_read \
	tools/p1_icm20608_read \
	collector/p1_collector

.PHONY: all clean

all: $(TOOLS)

tools/p1_outctl: tools/p1_outctl.c include/p1_protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

tools/p1_keyread: tools/p1_keyread.c include/p1_protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

tools/p1_ap3216c_read: tools/p1_ap3216c_read.c include/p1_protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

tools/p1_icm20608_read: tools/p1_icm20608_read.c include/p1_protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

collector/p1_collector: collector/p1_collector.c include/p1_protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) $(TOOLS)
