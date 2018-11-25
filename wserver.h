/* 
   A simple HTTP server in the internet domain using TCP
   The port number is passed as an argument 
   Stephen R. Williams, 25th Nov 2018
*/

void sock_talk(int, char[], char buffer[]);
void db_read(int sock);
void db_create();
void db_write(char *buff);
int db_count(char cmd[]);
void db_read_send_JSON(int sock, char cmd[]);
void error(const char *msg);
double get_temperature_reading();




