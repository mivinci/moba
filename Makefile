CC = gcc
CFLAGS = -llua


all: moba.so


moba.so: moba.o
	$(CC) -shared -o $@ $<


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<


.PHONY: test
test: test.c test.lua input.txt
	$(CC) $(CFLAGS) -o $@ test.c moba.c
	cat input.txt | ./$@
