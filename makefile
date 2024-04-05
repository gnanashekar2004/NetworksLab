server: simDNSServer.c
	gcc -Wall -o simDNSServer simDNSServer.c

client: simDNSClient.c
	gcc -Wall -o simDNSClient simDNSClient.c

exec: server client

run-server:
	./simDNSServer

run-client:
	./simDNSClient

clean:
	rm -f simDNSServer simDNSClient
