/* 
   A simple HTTP server in the internet domain using TCP and SQLite3
   The port number is passed as an argument 
   Stephen R. Williams, 25th Nov 2018
*/
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "wserver.h"

static int first, n_recs;
static int sock2;
extern char dbfn[];

// uses global variable "n_recs" to count up number of bytes to be sent
// uses global variable "first" to syncronise comma placement. Ugly but sqlite forced our hand
static int countback(void *NotUsed, int argc, char **argv, char **azColName)
{
    int i, n;
    char buff[256];
    
    switch(argc) 
    {
        case 2:
            if(first){
                sprintf(buff, "{\"x\":\"%s\",\"y\":%s}", argv[0], argv[1]); 
                first = 0;
            }
            else // prepend comma 
                sprintf(buff, ",{\"x\":\"%s\",\"y\":%s}", argv[0], argv[1]); 
            n_recs += strlen(buff);
            break;
        default:
            error("callback has invalid value for argc"); 
    }
    return 0;
}

// Dummy preperation of JSON data to get the number of bytes to be sent
// for use in the HTTP header, uses countback() function
int db_count(char cmd[])
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
      
    first = 1;
    n_recs = 0;
    rc = sqlite3_open(dbfn, &db);
    if(rc){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        error("Error: opening database");
    }   
    rc = sqlite3_exec(db, cmd, countback, 0, &zErrMsg);
    while(rc == SQLITE_BUSY){
        sleep(2);
        rc = sqlite3_exec(db, cmd, countback, 0, &zErrMsg);
    }
    if(rc != SQLITE_OK){
        printf("%d %d %d\n", rc, SQLITE_BUSY, SQLITE_LOCKED);
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    sqlite3_close(db);    
    return n_recs + 2;
}

// sends JSON data through socket with use of global variable "sock2"
// uses global variable "first" to syncronise comma placement. Ugly but sqlite forced our hand
static int callback1(void *NotUsed, int argc, char **argv, char **azColName)
{
    int i, n;
    char buff[256];
    
    switch(argc) 
    {
        case 1:
            n_recs = atoi(argv[0]);
            break;
        case 2:
            if(first){
                sprintf(buff, "{\"x\":\"%s\",\"y\":%s}", argv[0], argv[1]); 
                first = 0;
            }
            else  // prepend comma 
                sprintf(buff, ",{\"x\":\"%s\",\"y\":%s}", argv[0], argv[1]); 
            n = write(sock2, buff, strlen(buff));
            if (n < 0) error("Error: writing to socket");
            break;
        default:
            error("callback has invalid value for argc"); 
    }
    return 0;
}

// function to send JSON data. Example JSON data with two temperature readings:
// [{"x":"2017-07-08T06:15","y":23.2},{"x":"2017-07-10T16:20","y":15.3}]
// uses callback1() function

void db_read_send_JSON(int sock, char cmd[])
{
    int n;
    char buffer[256];
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
      
    sock2 = sock; // used in callback1 function
    // Simple HTTP header
    n = db_count(cmd);
    sprintf(buffer, "HTTP/1.1 200 OK\r\nServer: C_BBB\r\nContent-Length: %d", n);
    n = strlen(buffer);
    sprintf(buffer + n, "\r\nContent-Type: text/html; charset=utf-8\r\n\r\n");
    n = write(sock, buffer, strlen(buffer)); // send header to client
    if(n < 0) error("ERROR writing to socket");      
    // Header done 
    n = write(sock, "[", 1);
    if (n < 0) error("Error: writing to socket"); 
    first = 1;
    rc = sqlite3_open(dbfn, &db);
    if(rc){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        error("Error: opening database");
    }   
    rc = sqlite3_exec(db, cmd, callback1, 0, &zErrMsg);
    while(rc == SQLITE_BUSY){
        sleep(2);
        rc = sqlite3_exec(db, cmd, callback1, 0, &zErrMsg);
    }
    if(rc != SQLITE_OK){
        printf("%d %d %d\n", rc, SQLITE_BUSY, SQLITE_LOCKED);
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    n = write(sock, "]", 1);
    if (n < 0) error("Error: writing to socket"); 
    sqlite3_close(db);    
    //finished with database
    if (n < 0) error("Error: writing to socket");
}

void db_read(int sock)
{
    int n;
    char buffer[256];
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
      
    first = 1;
    n = read(sock, buffer, 255);
    //buffer[n] = '\0';
    sock2 = sock; // used in callback1 function
    rc = sqlite3_open(dbfn, &db);
    if(rc){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        error("Error: opening database");
    }   
    rc = sqlite3_exec(db, "select count(time) from tbl1", callback1, 0, &zErrMsg);
    while(rc == SQLITE_BUSY){
        sleep(2);
        rc = sqlite3_exec(db, "select count(time) from tbl1", callback1, 0, &zErrMsg);
    }
    rc = sqlite3_exec(db, "select * from tbl1", callback1, 0, &zErrMsg);
    while(rc == SQLITE_BUSY){
        sleep(2);
        rc = sqlite3_exec(db, "select * from tbl1", callback1, 0, &zErrMsg);
    }
    if(rc != SQLITE_OK){
        printf("%d %d %d\n", rc, SQLITE_BUSY, SQLITE_LOCKED);
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    sqlite3_close(db);    
    // finished with database
    n = write(sock, "database output done", 20);
    if (n < 0) error("Error: writing to socket");
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
        error("Error: opening database");
    }   
    rc = sqlite3_exec(db, buff, callback2, 0, &zErrMsg);
    while(rc == SQLITE_BUSY){
        printf("SQLITE_BUSSY flag returned\n");
        sleep(2);
        rc = sqlite3_exec(db, buff, callback2, 0, &zErrMsg);
    }
    if(rc != SQLITE_OK){
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
        error("Error: opening database");
    }   
    rc = sqlite3_exec(db, "create table tbl1(time varchar(23), temperature real)", callback2, 0, &zErrMsg);
    if(rc != SQLITE_OK){
        printf("%d %d %d\n", rc, SQLITE_BUSY, SQLITE_LOCKED);
        fprintf(stderr, "SQL Error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    sqlite3_close(db);   
}

