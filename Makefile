all: server

server: server.c
	cc -Wall -pedantic -lrt -g -pthread -o server server.c
