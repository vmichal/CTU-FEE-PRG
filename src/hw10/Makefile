CFLAGS+=  -pedantic -Wall -std=gnu11 -g
CC = clang

all: control

control: hw10-main.o protocol.o ringbuffer.o
	$(CC) $(CFLAGS) -lpthread hw10-main.c protocol.c ringbuffer.c -o control


clean:
	$(RM) *.o

