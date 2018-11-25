all: wserver

wserver: wserver.o db_read_write.o
	gcc -o wserver wserver.o db_read_write.o -lsqlite3

wsever.o: wserver.c wserver.h
	gcc -c wserver.c

db_read.o: db_read_write.c wserver.h
	gcc -c db_read_write.c 


