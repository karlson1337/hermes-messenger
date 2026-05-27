CXX = clang++

all:
	mkdir -p bin
	$(CXX) src/hermes_server.cpp -o bin/hermes_server -O2
	$(CXX) src/hermes_client.cpp -o bin/hermes_client -O2 -lreadline 

clean:
	rm -rf bin
