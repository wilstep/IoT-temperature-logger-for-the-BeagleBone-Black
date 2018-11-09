/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include "server.h"

// kill child
// killpg(pid, SIGKILL);

static void *shmem;
static char *run_flg;
static int *delay_time;
char dbfn[] = "temperature.db";

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno, pid;
    int n;
    char ch_flg;
    char buffer[256];
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    if(argc < 2) {
        fprintf(stderr,"Error: no port provided\n");
        exit(1);
    }
    signal(SIGCHLD,SIG_IGN); // stop accumulation of zombie processes
    if(access(dbfn, F_OK) == -1) db_create(); // create database if it doesn't exist
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) 
    error("Error: opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("Error: on binding");
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    shmem = create_shared_memory(sizeof(char) + sizeof(int)); // pointer to shared memory
    run_flg = shmem; 
    *run_flg = 0; // denotes that temperature logging is off
    delay_time = shmem + sizeof(char);
    *delay_time = 1;
    while(1){
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if(newsockfd < 0) error("Error: on accept");
        n = read(newsockfd, &ch_flg, 1); // read first byte of clients message, which is a flag
        if (n != 1) error("Error: reading from socket");
        pid = fork();
        if(pid < 0) error("Error: on fork");
        if(pid == 0){ // Child process
            close(sockfd); // copy of file descriptor, doesn't close parent's
            switch(ch_flg){
                case 'a': // run temperature monitoring
                    if(*run_flg){
                        sock_talk(newsockfd, "Temperature monitoring already running", buffer);
                        close(newsockfd);
                    }
                    else{
                        sock_talk(newsockfd, "Temperature monitoring started", buffer);
                        close(newsockfd);
                        sleep(2); // give 2 seconds for closing of previous start_temp...(), just in case
                        start_temperature_reading();  
                    }                
                    break;
                case 'b': // stop temperature monitoring
                    *run_flg = 0;
                    sock_talk(newsockfd, "Temperature monitoring switched off", buffer);
                    close(newsockfd);
                    break;  
                case 'c': // read data base
                    db_read(newsockfd);
                    close(newsockfd);
                    break; 
                case 'd': // alter time delay
                    n = read(newsockfd, buffer, 255);
                    buffer[n] = '\0';
                    *delay_time = atoi(buffer);
                    sprintf(buffer, "delay time set to %d", *delay_time);
                    n = write(newsockfd, buffer, strlen(buffer));
                    if (n < 0) error("Error: writing to socket");
                    close(newsockfd);
                    break; 
                case 'e': // shutdown
                    *run_flg = 0;
                    sock_talk(newsockfd, "Server is shutting down", buffer);
                    printf("Shutting down\n");
                    close(newsockfd); 
                    break;
                default:
                    fprintf(stderr, "Error: invalid flag passed from client\n");     
                    sock_talk(newsockfd, "Error: invalid flag passed from client", buffer);
                    close(newsockfd);           
            }
            _exit(0);
        }
        else{ // parent process
            close(newsockfd);
            if(ch_flg == 'e'){
                close(sockfd);
                return 0;
            }
        }
     } /* end of while */
     /* we never get here */ 
}

void error(const char *msg)
{
    perror(msg);
    exit(1);
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
      
    // now fork off proccess
    *run_flg = 1;
    while(1){
        while(time_left(*delay_time) > 2){
            sleep(1);
            if(!*run_flg) break;
        }
        if(!*run_flg) break;
        delay(*delay_time);
        T = get_temperature();
        t = time(0);   // get time now
        now = localtime(&t);
        sprintf(buff, "insert into tbl1 values('%.4d-%.2d-%.2dT%.2d:%.2d', %.2f)", now -> tm_year + 1900, 
                now -> tm_mon+1, now -> tm_mday, now -> tm_hour, now -> tm_min, T);            
        db_write(buff);
        sleep(10); // wait 10 secs as we are dealing in minutes, corrected for by delay() function
    }
    // continue with child process
}

/*** reads temperature from probe and returns value *****/
double get_temperature()
{
    int fd, i = 0;
    char str[256], *stp;
    char *fn = "/sys/bus/w1/devices/28-041750bdd9ff/w1_slave";
    
    fd = open(fn, O_RDONLY);
    //off_t offset = lseek(fd, 36, SEEK_SET);
    ssize_t size;
    do{
        if(i==256) break;
        size = read(fd, str + i++, 1);
    }while(size == 1);
    if(close(fd)) error("Error: the 1-wire driver file didn't close");
    str[i-1] = '\0';
    if(strstr(str, "YES") == NULL){
        error("Error: the temperature read failed");
        return 100000.0;
    }
    stp = strstr(str, "t=");
    stp += 2;
    i = atoi(stp);
    i = (i + 50) / 100; // round to nearest 0.1 degrees
    return (double) i * 0.1; 
}

/******* delays until the target time, except
for the case where the hour is crossed. Then the
delay is altered to coincide with the turn of the hour.
Note that the reading of the probe is slow, and that this
is corrected here.
*********************************************************/
void delay(int dmins) // dtime is delay time
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

/*** function to set up shared memory ***/
void* create_shared_memory(size_t size) 
{
    void *vp;  

    // Our memory buffer will be readable and writable:
    int protection = PROT_READ | PROT_WRITE;
    // The buffer will be shared (meaning other processes can access it), but
    // anonymous (meaning third-party processes cannot obtain an address for it),
    // so only this process and its children will be able to use it:
    int visibility = MAP_ANONYMOUS | MAP_SHARED;
    // The remaining parameters to `mmap()` are not important for this use case,
    // but the manpage for `mmap` explains their purpose.
    vp = mmap(NULL, size, protection, visibility, 0, 0);
    if(vp == MAP_FAILED){
        error("Error: virtual memory allocation failed");
    }
    return vp;
}



/******** sock_talk() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connection has been established and the
 first byte has been read.
 *****************************************/
void sock_talk(int sock, char mess[], char buffer[])
{
    int n;
      
    n = read(sock, buffer, 255);
    if (n < 0) error("Error: reading from socket");
    buffer[n] = '\0';
    n = write(sock, mess, strlen(mess));
    if (n < 0) error("Error: writing to socket");
}





