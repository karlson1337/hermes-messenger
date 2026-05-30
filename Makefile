CXX = clang++
CC = clang

all:
	mkdir -p bin
	cd src/server && $(CXX) server_main.cpp server_db.cpp -o ../../bin/server/hermes_server -O2 -std=c++17 -lsqlcipher -lsodium
	cd src/client && $(CC) client_main.c auth.c client_db.c -o ../../bin/client/hermes_client -std=c17 -O2 -lsqlcipher -lreadline -lsodium

clean:
	rm -rf bin
