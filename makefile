SHELL := /bin/bash

lib: msocket.o
	ar rcs libmsocket.a msocket.o

msocket.o: msocket.c msocket.h
	gcc -c -Wall -Wextra msocket.c

clean:
	rm -f libmsocket.a msocket.o
