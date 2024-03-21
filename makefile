SHELL := /bin/bash

msocket.o:
	gcc -c -Wall -Wextra msocket.c

lib: msocket.o
	ar rcs libmsocket.a msocket.o

initmsocket: initmsocket.c lib
	gcc -Wall -Wextra -o initmsocket initmsocket.c -L. -lmsocket -pthread

user1: user1.c lib
	gcc -Wall -Wextra -o user1 user1.c -L. -lmsocket -pthread

user2: user2.c lib
	gcc -Wall -Wextra -o user2 user2.c -L. -lmsocket -pthread

run: initmsocket
	./initmsocket

run1: user1
	./user1

run2: user2
	./user2

clean:
	rm -f libmsocket.a msocket.o initmsocket user1 user2