all: server client

server: server.o db_read_write.o
	gcc -o server server.o db_read_write.o -lsqlite3

sever.o: server.c server.h
	gcc -c server.c

db_read.o: db_read_write.c server.h
	gcc -c db_read_write.c 

client: client.c
	gcc -o client client.c

