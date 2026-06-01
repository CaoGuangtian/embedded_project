CC ?= arm-linux-gnueabihf-gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -O2
CPPFLAGS += -Iinclude

TARGET := device_agent
SRCS := \
	src/main.c \
	src/agent_config.c \
	src/net_client.c \
	src/protocol.c \
	src/status.c
OBJS := $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) $(TARGET) $(OBJS)
