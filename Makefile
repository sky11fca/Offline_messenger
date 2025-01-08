debug:
	gcc client.c -o client -g -lncurses
	gcc server.c -o server -g -lsqlite3 -lcrypto -lssl

all:
	gcc client.c -o client -lncurses
	gcc server.c -o server -lsqlite3 -lcrypto -lssl

clear:
	rm client server
