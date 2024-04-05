# Networking Assignment: simDNS Protocol Implementation

## Group members:
- Kandula Revanth (21CS10035)
- Pola Gnana Shekar (21CS10052)

## Description:
This project implements a simplified DNS (Domain Name System) protocol, called simDNS, using raw sockets in C. The simDNS protocol allows a client to send a query for the IP address corresponding to a domain name, and the server responds with the IP address.

## Instructions:

### 1. Compilation:
To compile the server and client executables, use the following command:
```bash
make exec
```

###2. Running the server:
To run the server, use the following command:
```bash
make run-server
```
Make sure to run the server before the client.

###3. Running the client:
To run client, use the following command in a different terminal:
```bash
make run-client
```
The Client will prompt you to input query string. Follow the specified format for the query string.

##Note:
- Run the server and Client in seperate Terminals.
- Ensure that the server is running before you run the client.