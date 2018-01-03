CC = gcc
CFLAGS =-Wall -Wextra -pedantic -std=gnu11 -D_GNU_SOURCE
LD_FLAGS = -lpthread

all : command peer name_server message_server

rebuild : clean all

csapp.o : csapp.c csapp.h
	$(CC) $(CFLAGS) -c $< -o $@

command : command.c command.h
	$(CC) $(CFLAGS) -c $< -o $@

message_server : message_server.c message_server.h csapp.o command.o
	$(CC) $(CFLAGS) -c $< -o $@

peer : peer.c peer.h message_server.o csapp.o command.o
	$(CC) $(CFLAGS) $< *.o -o $@ $(LD_FLAGS)

name_server : name_server.c name_server.h csapp.o command.o
	$(CC) $(CFLAGS) $< *.o -o $@ $(LD_FLAGS)


clean :
	rm -rf *.o peer name_server message_server

../src.zip :
	make clean
	cd .. && zip src.zip -r src
