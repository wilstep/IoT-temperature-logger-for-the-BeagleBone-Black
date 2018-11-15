/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

void error(const char *msg)
{
    perror(msg);
    printf("\n%s", msg);
    exit(1);
}

void file_serv(int sock, char fn[])
{
    char buffer[512];
    int n, c;
    FILE *ifp;
    
    ifp = fopen(fn, "r");
    fseek(ifp, 0L, SEEK_END);
    // get the size of the file in bytes, less the end of file character
    n = ftell(ifp) - 1;
    rewind(ifp);
    // Simple HTTP headers
    sprintf(buffer, "HTTP/1.1 200 OK\nServer: C_BBB\nContent-Length: %d", n);
    n = strlen(buffer);
    sprintf(buffer + n, "\nContent-Type: text/html; charset=utf-8\n\n");
    n = write(sock, buffer, strlen(buffer)); // send header to client
    if(n < 0) error("ERROR writing to socket");
    while(1){ // loop which sends the html file to the client
        n = 0;
        while((c = getc(ifp)) != EOF){
            buffer[n++] = c;
            if(n == 511) break;
        }
        if(n == 0) break; // end of file reached
        buffer[n] = '\0';
        n = write(sock, buffer, strlen(buffer));
        if (n < 0) error("ERROR writing to socket");
        if(n == 0) break; // The browser ain't buying what we're selling
    }
    fclose(ifp);
}

/*** reads temperature from probe and returns value *****/
double get_temperature_reading()
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

void get_temperature(int sock)
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

void run(int portno, int sockfd)
{
    int newsockfd;
    socklen_t clilen;
    char buffer[1024];
    struct sockaddr_in cli_addr;
    int n;
      
    listen(sockfd, 5);    // listen for connections
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen); // creates the new socket for the client
    if (newsockfd < 0) error("ERROR on accept");
    bzero(buffer, 1024);
    n = read(newsockfd, buffer, 1023);  
    if(n < 0) error("ERROR reading from socket");
    printf("Here is the HTTP header from the client:");  
    printf("%s", buffer);
    if(strncmp(buffer, "GET / HTTP/1.1", 14) == 0) file_serv(newsockfd, "web_client.html");    
    if(strncmp(buffer, "GET /favicon.ico", 16) == 0) file_serv(newsockfd, "favicon.ico");
    if(strncmp(buffer, "GET /?data", 10) == 0) get_temperature(newsockfd);
    close(newsockfd);
}

int main(int argc, char *argv[])
{
    int i;
    int portno, sockfd;
    struct sockaddr_in serv_addr;
      
    if(argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    portno = atoi(argv[1]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // creates socket
    if (sockfd < 0) error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno); // htons: host to network short
    // bind the socket to an address
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");
    while(1) run(portno, sockfd);
    close(sockfd);
    return 0; 
}


