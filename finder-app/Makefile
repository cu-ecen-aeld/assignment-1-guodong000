GCC = $(CROSS_COMPILE)gcc

all: writer

writer: writer.c
	$(GCC) -g -Wall -o writer writer.c

clean:
	rm -f *.o writer
