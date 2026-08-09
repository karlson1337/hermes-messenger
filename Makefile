CXX = g++
CC = gcc

all:
	mkdir -p bin/client
	mkdir -p bin/server
	cd src/server && $(CXX) server_main.cpp server_db.cpp -o ../../bin/server/hermes_server -O2 -std=c++17 -lsqlcipher -lsodium
	cd src/client && $(CC) client_main.c ui.c auth.c client_db.c -o ../../bin/client/hermes_client -std=c17 -O2 -lsqlcipher -lsodium -lncurses

clean:
	rm -rf bin
