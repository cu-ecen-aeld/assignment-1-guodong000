CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror
LIBS = -pthread -lrt

.PHONY: all clean

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f aesdsocket
