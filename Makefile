debug:
	gcc client.c -o client -g -lncurses
	gcc server.c -o server -g -lsqlite3

all:
	gcc client.c -o client 
	gcc server.c -o server -lsqlite3

clear:
	rm client server
