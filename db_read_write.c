/* 
   An IoT temperature logger in the internet domain using TCP and SQLite3
   Stephen R. Williams, 8th Dec 2018
   Open source GPLv3 as specified at repository
*/

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "wserver.h"

static int first, n_recs;
static const int nreps = 10;
static int sock2;
extern char dbfn[];

// uses global variable "n_recs" to count up number of bytes to be sent
// uses global variable "first" to syncronise comma placement. Ugly but sqlite forced our hand
static int countback_JSON(void *NotUsed, int argc, char **argv, char **azColName)
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
// for use in the HTTP header, uses countback_JSON() function
int db_count_JSON(char cmd[])
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
    rc = sqlite3_exec(db, cmd, countback_JSON, 0, &zErrMsg);
    while(rc == SQLITE_BUSY){
        sleep(2);
        rc = sqlite3_exec(db, cmd, countback_JSON, 0, &zErrMsg);
    }
    if(rc != SQLITE_OK){
        printf("a %d %d %d %s\n", rc, SQLITE_BUSY, SQLITE_LOCKED, cmd);
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    sqlite3_close(db);    
    return n_recs + 2;
}

// sends JSON data through socket with use of global variable "sock2"
// uses global variable "first" to syncronise comma placement. Ugly but sqlite forced our hand
static int callback_JSON(void *NotUsed, int argc, char **argv, char **azColName)
{
    int n;
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
            return 1;
    }
    return 0;
}

// function to send JSON data. Example JSON data with two temperature readings:
// [{"x":"2017-07-08T06:15","y":23.2},{"x":"2017-07-10T16:20","y":15.3}]
// uses callback_JSON() function

void db_read_send_JSON(int sock, char cmd[])
{
    int n;
    char buffer[256];
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
      
    sock2 = sock; // used in callback_JSON function
    // Simple HTTP header
    n = db_count_JSON(cmd);
    sprintf(buffer, "HTTP/1.1 200 OK\r\nServer: C_BBB\r\nContent-Length: %d", n);
    n = strlen(buffer);
    sprintf(buffer + n, "\r\nContent-Type: application/json; charset=utf-8\r\n\r\n");
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
    rc = sqlite3_exec(db, cmd, callback_JSON, 0, &zErrMsg);
    while(rc == SQLITE_BUSY){
        sleep(2);
        rc = sqlite3_exec(db, cmd, callback_JSON, 0, &zErrMsg);
    }
    if(rc != SQLITE_OK){
        printf("b %d %d %d\n", rc, SQLITE_BUSY, SQLITE_LOCKED);
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    n = write(sock, "]", 1);
    if (n < 0) error("Error: writing to socket"); 
    sqlite3_close(db);    
    //finished with database
}

static int callback_range(void *NotUsed, int argc, char **argv, char **azColName)
{
    int n;
    char buff[256];
    
    if(argc == 1){
        sprintf(buff, "%s", argv[0]);
        buff[10] = '\0';
        n = write(sock2, buff, strlen(buff));
        if(n < 0) error("Error: writing to socket");
    }
    else{
        error("callback has invalid value for argc"); 
        return 1;
    }
    return 0;    
}

// finds and sends (as JSON) the first and last date in the database
void db_read_send_range(int sock)
{
    // select min(time) from tbl1;
    // select max(time) from tbl1;
    int i, n;
    char buffer[256];
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
      
    sock2 = sock; // used in callback_range function
    // Simple HTTP header
    sprintf(buffer, "HTTP/1.1 200 OK\r\nServer: C_BBB\r\nContent-Length: %d", 39);
    n = strlen(buffer);
    sprintf(buffer + n, "\r\nContent-Type: application/json; charset=utf-8\r\n\r\n");
    n = write(sock, buffer, strlen(buffer)); // send header to client
    if(n < 0) error("ERROR writing to socket");      
    // Header done 
    n = write(sock, "{\"min\":\"", 8);
    if (n < 0) error("Error: writing to socket"); 
    rc = sqlite3_open(dbfn, &db);
    if(rc){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        error("Error: opening database");
    }   
    rc = sqlite3_exec(db, "select min(time) from tbl1", callback_range, 0, &zErrMsg);
    i = 0;
    while(rc == SQLITE_BUSY){
        sleep(2);
        rc = sqlite3_exec(db, "select min(time) from tbl1", callback_range, 0, &zErrMsg);
        if(i++ == nreps) break;
    }
    if(rc != SQLITE_OK){
        printf("c %d %d %d\n", rc, SQLITE_BUSY, SQLITE_LOCKED);
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    n = write(sock, "\",\"max\":\"", 9);
    if (n < 0) error("Error: writing to socket"); 
    
    rc = sqlite3_exec(db, "select max(time) from tbl1", callback_range, 0, &zErrMsg);
    i = 0;
    while(rc == SQLITE_BUSY){
        sleep(2);
        rc = sqlite3_exec(db, "select max(time) from tbl1", callback_range, 0, &zErrMsg);
        if(i++ == nreps) break;
    }
    if(rc != SQLITE_OK){
        printf("d %d %d %d\n", rc, SQLITE_BUSY, SQLITE_LOCKED);
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }    
    n = write(sock, "\"}", 2);
    if (n < 0) error("Error: writing to socket"); 
    sqlite3_close(db);    
    //finished with database
}

// uses global variable "n_recs" to count up number of bytes to be sent
static int countback_CSV(void *NotUsed, int argc, char **argv, char **azColName)
{
    int i, n;
    char buff[256];
    
    switch(argc) 
    {
        case 2:
            sprintf(buff, "%s,%s\n", argv[0], argv[1]); 
            n_recs += strlen(buff);
            break;
        default:
            error("callback has invalid value for argc"); 
    }
    return 0;
}

// Dummy preperation of CSV data to get the number of bytes to be sent
// for use in the HTTP header, uses countback_CSV() function
int db_count_CSV(char cmd[])
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
      
    n_recs = 0;
    rc = sqlite3_open(dbfn, &db);
    if(rc){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        error("Error: opening database");
    }   
    rc = sqlite3_exec(db, cmd, countback_CSV, 0, &zErrMsg);
    while(rc == SQLITE_BUSY){
        sleep(2);
        rc = sqlite3_exec(db, cmd, countback_CSV, 0, &zErrMsg);
    }
    if(rc != SQLITE_OK){
        printf("e %d %d %d\n", rc, SQLITE_BUSY, SQLITE_LOCKED);
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    sqlite3_close(db);    
    return n_recs;
}

// sends CSV data through socket with use of global variable "sock2"
static int callback_CSV(void *NotUsed, int argc, char **argv, char **azColName)
{
    int n;
    char buff[256];
    
    switch(argc) 
    {
        case 1:
            n_recs = atoi(argv[0]);
            break;
        case 2:
            sprintf(buff, "%s,%s\n", argv[0], argv[1]); 
            buff[10] = ',';
            n = write(sock2, buff, strlen(buff));
            if (n < 0) error("Error: writing to socket");
            break;
        default:
            error("callback has invalid value for argc"); 
    }
    return 0;
}

void db_read_send_CSV(int sock, char cmd[])
{
    int n;
    char buffer[256];
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
      
    sock2 = sock; // used in callback_CSV function
    // Simple HTTP header
    n = db_count_CSV(cmd) + 22;
    sprintf(buffer, "HTTP/1.1 200 OK\r\nServer: C_BBB\r\nContent-Length: %d", n);
    n = strlen(buffer);
    sprintf(buffer + n, "\r\nContent-Type: text/csv; charset=utf-8\r\n\r\n");
    n = write(sock, buffer, strlen(buffer)); // send header to client
    if(n < 0) error("Error: writing to socket");      
    // Header done 
    n = write(sock, "date,time,temperature\n", 22);
    rc = sqlite3_open(dbfn, &db);
    if(rc){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        error("Error: opening database");
    }   
    rc = sqlite3_exec(db, cmd, callback_CSV, 0, &zErrMsg);
    while(rc == SQLITE_BUSY){
        sleep(2);
        rc = sqlite3_exec(db, cmd, callback_CSV, 0, &zErrMsg);
    }
    if(rc != SQLITE_OK){
        printf("f %d %d %d\n", rc, SQLITE_BUSY, SQLITE_LOCKED);
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    if (n < 0) error("Error: writing to socket"); 
    sqlite3_close(db);    
    //finished with database
}


static int callback(void *NotUsed, int argc, char **argv, char **azColName)
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
    rc = sqlite3_exec(db, buff, callback, 0, &zErrMsg);
    while(rc == SQLITE_BUSY){
        printf("SQLITE_BUSSY flag returned\n");
        sleep(2);
        rc = sqlite3_exec(db, buff, callback, 0, &zErrMsg);
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
    rc = sqlite3_exec(db, "create table tbl1(time varchar(23), temperature real)", callback, 0, &zErrMsg);
    if(rc != SQLITE_OK){
        printf("g %d %d %d\n", rc, SQLITE_BUSY, SQLITE_LOCKED);
        fprintf(stderr, "SQL Error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    sqlite3_close(db);   
}

