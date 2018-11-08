#include <stdio.h>
#include <sqlite3.h>
#include <unistd.h>
#include <string.h>
#include "server.h"

static int sock2;
extern char dbfn[];

//strchr()

static int callback1(void *NotUsed, int argc, char **argv, char **azColName)
{
    int i, n;
    char buff[256];
    
    //for(i=0; i<argc; i++){
    //    printf("%d Bla %s = %s\n", i, azColName[i], argv[i] ? argv[i] : "NULL");
    //}
    //printf("\n");
    sprintf(buff, "%s", argv[0]);
    for(i=1; i<argc; ++i){
        n = strlen(buff);
        sprintf(buff + n, ", %s", argv[i]);    
    }
    n = strlen(buff);
    buff[n] = '\n';
    buff[n + 1] = '\0';
    n = write(sock2, buff, strlen(buff));
    if (n < 0) error("ERROR writing to socket");
    return 0;
}


void db_read(int sock)
{
    int n;
    char buffer[256];
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
      
    //bzero(buffer,256);
    n = read(sock, buffer, 255);
    if (n < 0) error("ERROR reading from socket");
    buffer[n] = '\0';
    printf("Here is the message: %s", buffer);
    // query database and send results back to client
    sock2 = sock; // used in callback1 function
    rc = sqlite3_open(dbfn, &db);
    if(rc){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        error("Error opening database");
    }   
    rc = sqlite3_exec(db, "select count(time) from tbl1", callback1, 0, &zErrMsg);
    while(rc == SQLITE_BUSY){
        rc = sqlite3_exec(db, "select count(time) from tbl1", callback1, 0, &zErrMsg);
        sleep(2);
    }
    rc = sqlite3_exec(db, "select * from tbl1", callback1, 0, &zErrMsg);
    while(rc == SQLITE_BUSY){
        rc = sqlite3_exec(db, "select * from tbl1", callback1, 0, &zErrMsg);
        sleep(2);
    }
    if(rc != SQLITE_OK){
        printf("%d %d %d\n", rc, SQLITE_BUSY, SQLITE_LOCKED);
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    sqlite3_close(db);    
    // finished with database
    n = write(sock, "database output done", 20);
    if (n < 0) error("ERROR writing to socket");
}

static int callback2(void *NotUsed, int argc, char **argv, char **azColName)
{
    printf("%s, %s\n", argv[0], argv[1]);
    return 0;
}

void db_write(char *buff)
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
      
    // query database and send results back to client
    rc = sqlite3_open(dbfn, &db);
    if(rc){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        error("Error opening database");
    }   
    rc = sqlite3_exec(db, buff, callback2, 0, &zErrMsg);
    while(rc == SQLITE_BUSY){
        rc = sqlite3_exec(db, buff, callback2, 0, &zErrMsg);
        sleep(2);
    }
    if(rc != SQLITE_OK){
        printf("%d %d %d\n", rc, SQLITE_BUSY, SQLITE_LOCKED);
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    sqlite3_close(db);    
}

void db_create()
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
      
    // query database and send results back to client
    rc = sqlite3_open(dbfn, &db);
    if(rc){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        error("Error opening database");
    }   
    rc = sqlite3_exec(db, "create table tbl1(time varchar(23), temperature real)", callback2, 0, &zErrMsg);
    if(rc != SQLITE_OK){
        printf("%d %d %d\n", rc, SQLITE_BUSY, SQLITE_LOCKED);
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    sqlite3_close(db);   
}

