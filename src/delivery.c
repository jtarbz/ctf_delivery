#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 4747
#define RECV_SIZE 64

bool interrupt = false;	// this guy lets the main loop know when it's time to shut down ***gracefully***

/* custom signal handler for graceful shutdown purposes */
void handle_signal(int sig_type) {
	printf("beginning clean server shutdown due to interrupt ...\n");
	interrupt = true;
	return;
}

/* accept a client waiting its turn  */
int accept_client(int server_socket) {
	int client_socket;
	struct sockaddr_in client_addr;
	unsigned int client_length;

	/* here's the actual accepting part */
	if((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_length)) < 0) {
		if(interrupt == true) return -1;	// is a nested conditional the best way to do this?
		fprintf(stderr, "failed to accept a client\n");
		return -1;
	}

	printf("handling client %s\n", inet_ntoa(client_addr.sin_addr));
	return client_socket;
}

/* receive data from the client and then compare it to some flag / generated data; customize this function as needed*/
void handle_client(int client_socket) {
	unsigned char recv_buffer[RECV_SIZE];
	unsigned int recvd_msg_size = 0;

	/* receive only one message up to (RECV_SIZE - 1) bytes in size */
	do {
		if(interrupt == true) {	// implement graceful shutdown
			close(client_socket);
			return;
		}

		if((recvd_msg_size += recv(client_socket, recv_buffer + recvd_msg_size, RECV_SIZE - 1, 0)) < 0) {
			fprintf(stderr, "failed to receive from client socket: accepting next connection ...\n");
			close(client_socket);
			return;
		}
	} while(recvd_msg_size < 0);

	recv_buffer[recvd_msg_size] = '\0';     // always end your sentence ;)
	printf("%s", recv_buffer);      // simple action for now, will change
	if((strcmp(recv_buffer, "flag{flag}\n")) != 0) send(client_socket, "wrong flag!\n", 16, 0);
	else send(client_socket, "correct flag!\n", 16, 0);

	close(client_socket);
}

int main(void) {
	int client_socket, server_socket;
	struct sockaddr_in server_addr;

	/* set custom handler for SIGINT */
	struct sigaction handler;
	handler.sa_handler = handle_signal;
	if(sigfillset(&handler.sa_mask) < 0) {
		fprintf(stderr, "failed to set signal masks\n");
		exit(1);
	}
	handler.sa_flags = 0;	// no sa_flags
	if(sigaction(SIGINT, &handler, 0) < 0) {
		fprintf(stderr, "failed to set new handler for SIGINT\n");
		exit(1);
	}

	/* spawn socket and prepare it for binding */
	if((server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		fprintf(stderr, "failed to create server socket\n");
		exit(1);
	}

	if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {	// be vanquished, foul error!
		fprintf(stderr, "failed to set reusable option on listening socket\n");
		exit(1);
	}

	memset(&server_addr, 0, sizeof server_addr);	// zero out to prevent ~~BAD VOODOO~~
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(PORT);

	/* bind and listen */
	if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof server_addr) < 0) {
		fprintf(stderr, "failed to bind socket to port\n");
		exit(1);
	}

	if(listen(server_socket, 64) < 0) {
		fprintf(stderr, "server socket failed to listen\n");
		exit(1);
	}

	/* main loop (probably just implement one client to start, and then expand) */
	while(interrupt != true) if((client_socket = accept_client(server_socket)) >= 0) handle_client(client_socket);	// you can make this more sophisticated later

	/* execute upon interrupt */
	close(server_socket);
	exit(0);
}
