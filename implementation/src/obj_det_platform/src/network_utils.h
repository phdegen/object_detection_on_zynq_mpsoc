#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#define BUFFSIZE 32
#define IMG_SIZE 1232*374*3
#define IMG_BUFFSIZE 1382400
#define SENDSIZE 1024 //32768



int setup_conn();

int send_image(char * image);

int close_conn();
