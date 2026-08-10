CXX = g++
CC = gcc

BREW_PREFIX := $(shell brew --prefix)
MACOS_INCLUDES = -I$(BREW_PREFIX)/include
MACOS_LDFLAGS  = -L$(BREW_PREFIX)/lib

linux:
	mkdir -p bin/client
	mkdir -p bin/server
	cd src/server && $(CXX) server_main.cpp server_db.cpp -o ../../bin/server/hermes_server -O2 -std=c++17 -lsqlcipher -lsodium
	cd src/client && $(CC) client_main.c ui.c auth.c client_db.c -o ../../bin/client/hermes_client -std=c17 -O2 -lsqlcipher -lsodium -lncurses

macos:
	mkdir -p bin/client
	mkdir -p bin/server
	cd src/server && $(CXX) server_main.cpp server_db.cpp -o ../../bin/server/hermes_server -O2 -std=c++17 $(MACOS_INCLUDES) $(MACOS_LDFLAGS) -lsqlcipher -lsodium
	cd src/client && $(CC) client_main.c ui.c auth.c client_db.c -o ../../bin/client/hermes_client -std=c17 -O2 $(MACOS_INCLUDES) $(MACOS_LDFLAGS) -lsqlcipher -lsodium -lncurses

clean:
	rm -rf bin
