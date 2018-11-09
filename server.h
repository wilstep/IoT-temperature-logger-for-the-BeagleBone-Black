void sock_talk(int, char[], char buffer[]);
void db_read(int sock);
void db_create();
void db_write(char *buff);
void error(const char *msg);
void start_temperature_reading();
void* create_shared_memory(size_t size);
double get_temperature();
void delay(int);
int time_left(int);




