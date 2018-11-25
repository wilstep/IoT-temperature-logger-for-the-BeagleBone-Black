/* 
   A simple HTTP server in the internet domain using TCP and SQLite3
   The port number is passed as an argument 
   Stephen R. Williams, 25th Nov 2018
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include "wserver.h"

const int delay_time = 30;
char dbfn[] = "temperature.db";

void error(const char *msg)
{
    perror(msg);
    printf("%s\n", msg); 
    exit(1);
}

// returns the seconds left until next target time 
int time_left(int dmins) // dtime is delay time
{
    time_t t;
    struct tm *now;
    int secs, secs_left;
    const int delay_mins = dmins;
    
    t = time(NULL);
    now = localtime(&t);
    // next line: removes the extra time which has been lost
    secs = delay_mins * 60 - now -> tm_sec - 60 * (now -> tm_min % delay_mins);
    secs_left = 3600 - 60 * now -> tm_min - now -> tm_sec; // seconds left in hour
    if(secs_left < secs) secs = secs_left; 
    return secs;
}

/******* delays until the target time, except
for the case where the hour is crossed. Then the
delay is altered to coincide with the turn of the hour.
Note that the reading of the probe is slow, and that this
is corrected here.
*********************************************************/
void delay(int dmins) 
{
    time_t t;
    struct tm *now;
    int secs, secs_left;
    const int delay_mins = dmins;
    
    t = time(NULL);
    now = localtime(&t);
    // next line: removes the extra time which has been lost
    secs = delay_mins * 60 - now -> tm_sec - 60 * (now -> tm_min % delay_mins);
    secs_left = 3600 - 60 * now -> tm_min - now -> tm_sec; // seconds left in hour
    if(secs_left < secs) secs = secs_left; 
    sleep(secs);
}

/*** start_temperature_reading() ***********
 This function starts a child process
 to periodically read the temperature and then to record
 the results in the database. Handles required
 socket communication
 *****************************************/
void start_temperature_reading()
{
    char buff[256];
    double T;
    time_t t = 0;
    struct tm *now;
      
    //*run_flg = 1;
    while(1){
        while(time_left(delay_time) > 2){
            sleep(1);
            //if(!*run_flg) break;
        }
        //if(!*run_flg) break;
        delay(delay_time);
        T = get_temperature_reading();
        t = time(0);   // get time now
        now = localtime(&t);
        sprintf(buff, "insert into tbl1 values('%.4d-%.2d-%.2dT%.2d:%.2d', %.2f)", now -> tm_year + 1900, 
                now -> tm_mon+1, now -> tm_mday, now -> tm_hour, now -> tm_min, T);            
        db_write(buff);
        sleep(2); // wait 2 secs as we are dealing in minutes, corrected for by delay() function
    }
    // continue with child process
}

// input function for "scandir()" function in "get_temperature_reading()"
int filter(const struct dirent *name)
{
    if(strncmp("28-", name->d_name, 3) == 0)
        return 1;
    return 0;
}

/*** reads temperature from probe and returns value *****/
double get_temperature_reading()
{
    int fd, i = 0;
    char str[256], *stp;
    char fn[512];
    struct dirent **namelist;
    int n;
    
    // get the device with the serial number of W1 temperature sensor
    n = scandir("/sys/bus/w1/devices", &namelist, filter, alphasort);
    if(n <= 0){
        printf("temperature device not found");
        return 100000.0;
    }
    else{
        sprintf(fn, "/sys/bus/w1/devices/%s/w1_slave", namelist[0]->d_name);
        while(n--) free(namelist[n]);
        free(namelist);
    } // got it
    fd = open(fn, O_RDONLY);
    if(fd == -1) printf("Could open temperture probe driver file: %s", fn);
    //off_t offset = lseek(fd, 36, SEEK_SET);
    ssize_t size;
    do{
        if(i==256) break;
        size = read(fd, str + i++, 1);
    }while(size == 1);
    if(close(fd)) error("Error: the 1-wire driver file didn't close");
    str[i-1] = '\0';
    if(strstr(str, "YES") == NULL){
        printf("Error: the temperature read failed\n");
        return 100000.0;
    }
    stp = strstr(str, "t=");
    stp += 2;
    i = atoi(stp);
    i = (i + 50) / 100; // round to nearest 0.1 degrees
    return (double) i * 0.1; 
}

// send the time and the temperature out to the client
void send_temperature(int sock)
{
    char buffer1[512], buffer2[256];
    int n, c;
    time_t t;
    struct tm *now;
 
    // set up buffer for later data transfer  
    t = time(NULL);
    now = localtime(&t);    
    sprintf(buffer2, "The time is %.2d:%.2d:%.2d", now -> tm_hour, now -> tm_min, now -> tm_sec); 
    n = strlen(buffer2);
    // next line calls get_temperature_reading() which gets temperature from probe
    sprintf(buffer2 + n, ", and the temperature is %.1f degrees Celsius", get_temperature_reading());  
    // Simple HTTP headers
    sprintf(buffer1, "HTTP/1.1 200 OK\nServer: C_BBB\nContent-Length: %lu", strlen(buffer2)); // This number: Bad!
    n = strlen(buffer1);
    sprintf(buffer1 + n, "\nContent-Type: text/html; charset=utf-8\n\n");
    n = write(sock, buffer1, strlen(buffer1)); // send header to client
    if(n < 0) error("ERROR writing to socket");
    n = write(sock, buffer2, strlen(buffer2)); // send data to client
    if(n < 0) error("ERROR writing to socket");
}



void file_serv(int sock, char fn[])
{
    char buffer[512];
    int n, c, cnt;
    FILE *ifp;
    
    if((ifp = fopen(fn, "r")) == NULL){
        sprintf(buffer, "Error, couldn't open file: %s", fn); 
        error(buffer);
    }
    fseek(ifp, 0L, SEEK_END);
    // get the size of the file in bytes
    n = ftell(ifp);
    rewind(ifp);
    // Simple HTTP header
    sprintf(buffer, "HTTP/1.1 200 OK\r\nServer: C_BBB\r\nContent-Length: %d", n);
    n = strlen(buffer);
    sprintf(buffer + n, "\r\nContent-Type: text/html; charset=utf-8\r\n\r\n");
    n = write(sock, buffer, strlen(buffer)); // send header to client
    if(n < 0) error("ERROR writing to socket");
    cnt = 0;
    while((n = fread(buffer, sizeof(char), 512, ifp)) != 0){ // loop which sends the html 
        cnt += n;
        n = write(sock, buffer, n);
        if (n < 0) error("ERROR writing to socket");
        if(n == 0) break; // The browser ain't buying what we're selling
    }
    fclose(ifp);
}

/* function to set up temperature reading, forks off proccess
to periodically read temperature */
void run_temp()
{
    int pid;
    
    pid = fork();
    if(pid < 0) error("ERROR on fork");
    if(pid == 0){ // child proccess
        start_temperature_reading();
        exit(0); // end child proccess
    }  
}


// switcher function, based on message sent from client (browser)
// Only responds to specific request strings, makes more secure
void run(int portno, int sockfd)
{
    char buffer[1024];
    int n;
      
    bzero(buffer, 1024);
    n = read(sockfd, buffer, 1023);  
    if(n < 0) error("ERROR reading from socket");
    if(strncmp(buffer, "GET / HTTP/1.1", 14) == 0){
        file_serv(sockfd, "graph.html");
        return;
    }    
    if(strncmp(buffer, "GET /favicon.ico", 16) == 0){
        file_serv(sockfd, "favicon.ico");
        return;
    }
    if(strncmp(buffer, "GET /graph.js", 13) == 0){
        file_serv(sockfd, "graph.js");
        return;
    }
    if(strncmp(buffer, "GET /data.json", 14) == 0){ 
        db_read_send_JSON(sockfd, "select * from tbl1");
        return;
    }
    if(strncmp(buffer, "GET /?data", 10) == 0){
        send_temperature(sockfd);
        return;
    }
    printf("invalid request made: %s: ", buffer);
}


int main(int argc, char *argv[])
{
    int i, pid;
    socklen_t clilen;
    int portno, sockfd, newsockfd;
    int pid_file, rc;
    struct sockaddr_in serv_addr, cli_addr;
      
    if(argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    // make sure program is only running once in current directory
    pid_file = open("single.pid", O_CREAT | O_RDWR, 0666);
    rc = flock(pid_file, LOCK_EX | LOCK_NB);
    if(rc){
        if(EWOULDBLOCK == errno)
            error("program is already running in this directory, Bye!");// another instance is running
    }
    // program is only running once
    signal(SIGCHLD,SIG_IGN); // stop accumulation of zombie processes (POSIX UNIX)
    if(access(dbfn, F_OK) == -1) db_create(); // create database if it doesn't exist
    run_temp(); // forks off proccess to periodically measure and log temperature
    portno = atoi(argv[1]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // creates socket
    if (sockfd < 0) error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno); // htons: host to network short
    // bind the socket to an address
    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");
    listen(sockfd, 5);    // listen for connections
    clilen = sizeof(cli_addr);
    while(1){
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen); // creates the new socket for the client 
        if (newsockfd < 0) error("ERROR on accept");
        pid = fork();
        if(pid < 0) error("ERROR on fork");
        if(pid == 0){ // child proccess
            close(sockfd);
            run(portno, newsockfd);
            exit(0); // end child proccess
        } 
        close(newsockfd);
    }
    close(sockfd);
    return 0; 
}


