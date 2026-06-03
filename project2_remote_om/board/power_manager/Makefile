CC ?= arm-linux-gnueabihf-gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -O2
CPPFLAGS += -Iinclude

TARGET := power_manager
SRCS := \
	src/main.c \
	src/power_config.c \
	src/power_log.c \
	src/power_mode.c
OBJS := $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) $(TARGET) $(OBJS)

