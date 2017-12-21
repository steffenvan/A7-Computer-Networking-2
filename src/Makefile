CC = gcc
CFLAGS =-Wall -Wextra -pedantic -std=gnu11
LD_FLAGS = -lpthread

all : command peer name_server

rebuild : clean all

csapp.o : csapp.c csapp.h
	$(CC) $(CFLAGS) -c $< -o $@

command : command.c command.h
	$(CC) $(CFLAGS) -c $< -o $@

peer : peer.c peer.h csapp.o command.o
	$(CC) $(CFLAGS) $< *.o -o $@ $(LD_FLAGS)

name_server : name_server.c name_server.h csapp.o command.o
	$(CC) $(CFLAGS) $< *.o -o $@ $(LD_FLAGS)

clean :
	rm -rf *.o peer name_server

../src.zip :
	make clean
	cd .. && zip src.zip -r src
