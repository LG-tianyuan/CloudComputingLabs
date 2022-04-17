CC = gcc
CPPFLAGS = -O2 -pthread

all:httpserver

httpserver:httpserver.c threadpool.c

clean:
	rm httpserver

v2:
	gcc httpserver_v2.c threadpool.c -O2 -pthread -o httpserver

ceshi:
	gcc httpserver_test.c threadpool.c -O2 -pthread -o httpserver

test:
	gcc test.c -o test
clean_test:
	rm test