CC = gcc

all: server client

server: simDNSServer.c
	$(CC) -o server simDNSServer.c

client: simDNSClient.c
	$(CC) -o client simDNSClient.c

run-server: server
	sudo ./server

run-client: client
	sudo ./client

clean:
	rm -f server client