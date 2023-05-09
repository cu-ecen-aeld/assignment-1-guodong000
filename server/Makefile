CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror

.PHONY: all clean

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f aesdsocket
