/* 
   An IoT temperature logger in the internet domain using TCP and SQLite3
   Stephen R. Williams, 8th Dec 2018
   Open source GPLv3 as specified at repository
*/

void sock_talk(int, char[], char buffer[]);
void db_read(int sock);
void db_create();
void db_write(char *buff);
int db_count_JSON(char cmd[]);
void db_read_send_JSON(int sock, char cmd[]);
void db_read_send_CSV(int sock, char cmd[]);
void db_read_send_range(int sock);
void error(const char *msg);
double get_temperature_reading();




