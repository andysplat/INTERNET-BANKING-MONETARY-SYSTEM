build: server client
server: server.cpp
	g++ -Wall -std=c++11 server.cpp -o server
client: client.c
	gcc -Wall client.c -o client
clean:
	rm client server
