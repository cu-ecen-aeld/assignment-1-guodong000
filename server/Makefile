CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror
LDFLAGS ?= -pthread -lrt
TARGET ?= aesdsocket

.PHONY: all clean

all: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)
