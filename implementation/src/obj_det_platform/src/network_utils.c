#include "network_utils.h"

char ip_adress[] = "129.132.4.131";
char port[] = "1346";

static int sock;
static struct sockaddr_in server;
static char buffer[BUFFSIZE];
static int received = 0;

void Die(char *mess) { perror(mess); exit(1); }

int setup_conn(){
	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		Die("Failed to create socket");
	}
	/* Construct the server sockaddr_in structure */
	memset(&server, 0, sizeof(server));       /* Clear struct */
	server.sin_family = AF_INET;                  /* Internet/IP */
	server.sin_addr.s_addr = inet_addr(ip_adress);  /* IP address */
	server.sin_port = htons(atoi(port));       /* server port */
	/* Establish connection */
	if (connect(sock,
			(struct sockaddr *) &server,
			sizeof(server)) < 0) {
		Die("Failed to connect with server");
	}

	return 0;
}

int send_image(char * image){
	int i;

	sprintf(buffer, "STRT");
	if (send(sock, buffer, 4, 0) != 4) {
		Die("Mismatch in number of sent bytes");
	}


	for(i=0;i<IMG_SIZE-SENDSIZE;i+=SENDSIZE){
		if (send(sock, image+i, SENDSIZE, 0) != SENDSIZE) {
			Die("Mismatch in number of sent bytes");
		}
	}
	printf("last send size is %d",send(sock, image+i, IMG_SIZE-i, 0));


	fprintf(stdout, "Received: ");
	while (received < 6) {
		int bytes = 0;
		if ((bytes = recv(sock, buffer, 6, 0)) < 1) {
			//Die("Failed to receive bytes from server");
		}
		received += bytes;
		buffer[bytes] = '\0';        //Assure null terminated string
		fprintf(stdout, buffer);
	}
	fprintf(stdout, "\n");
	received=0;

	return 0;
}

int close_conn(){

	sprintf(buffer, "STOP");
	if (send(sock, buffer, 4, 0) != 4) {
		Die("Mismatch in number of sent bytes");
	}


	close(sock);

	return 0;
}
