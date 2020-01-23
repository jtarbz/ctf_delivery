#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include "delivery.h"

#define MAX_MODULES 64

/* structure for holding module information */
typedef struct module {
	char name[MODULE_NAME_LENGTH];
	int fd;
	int port;
	int pid;
	bool state;
} module;

/* start a module given its file descriptor, and return the resulting process' pid */
int start_module(int fd, char *name, FILE *error_file) {
	int pid = fork();
	char *dumb_required_argument[] = {name, 0};
	
	if(pid == 0) {
		fexecve(fd, dumb_required_argument, dumb_required_argument);	// instead of actually passing an environment, we'll just pretend that we did
		/* successful call never returns ... */
		fprintf(error_file, "call to fexecve returned, meaning that something went horribly wrong\n");
		exit(1);
	}
	
	return pid;
}

/* receive data from the client and return a structure containing operation information and argument */ 
int handle_input(int server_socket, struct sockaddr_un client_address, module *module_registry, char *answer, FILE *error_file) {
	input client_input;	// operation and argument sent from client
	char end = 127;	// END code to let client know communications are over
	
	if(recvfrom(server_socket, &client_input, sizeof(input), 0, NULL, NULL) < 0) {
		fprintf(error_file, "failed to receive from datagram socket\n");
		return 0;
	}
	
	/* poll to see if any children have changed state before performing an operation */
	for(int i = 0; module_registry[i].fd != 0; ++i) {
		if(module_registry[i].pid != 0) {
			if(waitpid(module_registry[i].pid, NULL, WNOHANG) > 0) {
				module_registry[i].state = false;
				module_registry[i].pid = 0;
			}
		} 
	}
	
	/* switch case for input handling - refer to operator enum type */
	switch(client_input.op) {
		case 0:		// start module by name
			for(int i = 0; module_registry[i].fd != 0; ++i) {
				if(strcmp(module_registry[i].name, client_input.arg) == 0) {
					if(module_registry[i].state == false) {
						module_registry[i].pid = start_module(module_registry[i].fd, module_registry[i].name, error_file);
						module_registry[i].state = true;
						
					}					
					else break;
				}
			}
			
			if(sendto(server_socket, &end, sizeof(char), 0, (struct sockaddr *)&client_address, sizeof(struct sockaddr_un)) < 0) fprintf(error_file, "failed to send message to client\n");

			break;
		
		case 1:		// start all modules
			for(int i = 0; module_registry[i].fd != 0; ++i) {
				if(module_registry[i].state == false) { 
					module_registry[i].pid = start_module(module_registry[i].fd, module_registry[i].name, error_file);
					module_registry[i].state = true;
				}					 
			}
			
			if(sendto(server_socket, &end, sizeof(char), 0, (struct sockaddr *)&client_address, sizeof(struct sockaddr_un)) < 0) fprintf(error_file, "failed to send message to client\n");
			
			break;
		
		case 2:		// stop module by name
			for(int i = 0; module_registry[i].fd != 0; ++i) {
				if(strcmp(module_registry[i].name, client_input.arg) == 0) {
					if(module_registry[i].state == true) {
						if(kill(module_registry[i].pid, SIGTERM) < 0) {
							fprintf(error_file, "failed to kill a child process\n");
							break;
						}
						
						module_registry[i].state = false;
					}
					
					else break;
				}
			}
			
			if(sendto(server_socket, &end, sizeof(char), 0, (struct sockaddr *)&client_address, sizeof(struct sockaddr_un)) < 0) fprintf(error_file, "failed to send message to client\n");

			break;
		
		case 3:		// stop module by pid
			for(int i = 0; module_registry[i].fd != 0; ++i) {
				if(module_registry[i].pid == atoi(client_input.arg) && module_registry[i].state == true) {
					if(kill(module_registry[i].pid, SIGTERM) < 0) {
						fprintf(error_file, "failed to kill a child process\n");
						break;
					}
					
					module_registry[i].state = false;
					break;
				}
			}
		
			if(sendto(server_socket, &end, sizeof(char), 0, (struct sockaddr *)&client_address, sizeof(struct sockaddr_un)) < 0) fprintf(error_file, "failed to send message to client\n");

			break;
		
		case 4:		// stop all modules
			for(int i = 0; module_registry[i].fd != 0; ++i) {
				if(module_registry[i].state == true) {
					if(kill(module_registry[i].pid, SIGTERM) < 0) {
						fprintf(error_file, "failed to kill a child process\n");
						break;
					}
					
					module_registry[i].state = false;
				}
			
			}
			
			if(sendto(server_socket, &end, sizeof(char), 0, (struct sockaddr *)&client_address, sizeof(struct sockaddr_un)) < 0) fprintf(error_file, "failed to send message to client\n");
			
			break;
			
		case 5: 	// restart module by name
			for(int i = 0; module_registry[i].fd != 0; ++i) {
				if(strcmp(module_registry[i].name, client_input.arg) == 0) {
					if(module_registry[i].state == true) {
						if(kill(module_registry[i].pid, SIGTERM) < 0) {
							fprintf(error_file, "failed to kill a child process during restart\n");
							break;
						}
						
						module_registry[i].pid = start_module(module_registry[i].fd, module_registry[i].name, error_file);
					}
				}
			}
			
			if(sendto(server_socket, &end, sizeof(char), 0, (struct sockaddr *)&client_address, sizeof(struct sockaddr_un)) < 0) fprintf(error_file, "failed to send message to client\n");

			break;
			
		case 6: 	// restart module by pid
			for(int i = 0; module_registry[i].fd != 0; ++i) {
				if(module_registry[i].pid == atoi(client_input.arg)) {
					if(module_registry[i].state == true) {	
						if(kill(module_registry[i].pid, SIGTERM) < 0) {
							fprintf(error_file, "failed to kill a child process during restart\n");
							break;
						}
						
						module_registry[i].pid = start_module(module_registry[i].fd, module_registry[i].name, error_file);
					}
				}
			}
			
			if(sendto(server_socket, &end, sizeof(char), 0, (struct sockaddr *)&client_address, sizeof(struct sockaddr_un)) < 0) fprintf(error_file, "failed to send message to client\n");
		
			break;
					
		case 7: 	// shutdown the daemon
			if(sendto(server_socket, &end, sizeof(char), 0, (struct sockaddr *)&client_address, sizeof(struct sockaddr_un)) < 0) fprintf(error_file, "failed to send message to client\n");

			return -1;
		
		case 8:		// return a general status for the daemon
			/* systematically print and send module names, ports, and states/pids */
			for(int i = 0; module_registry[i].fd != 0; ++i) {
				snprintf(answer, 128, "%s:%d active: %d\n", module_registry[i].name, module_registry[i].port, module_registry[i].state == true ? module_registry[i].pid : false);
				if(sendto(server_socket, answer, 128, 0, (struct sockaddr *)&client_address, sizeof(struct sockaddr_un)) < 0) fprintf(error_file, "failed to send message to client\n");
			}			
			
			if(sendto(server_socket, &end, sizeof(char), 0, (struct sockaddr *)&client_address, sizeof(struct sockaddr_un)) < 0) fprintf(error_file, "failed to send message to client\n");

			break;
		
		default:	// something got messed up big time client-side
			fprintf(error_file, "unknown operator passed by client - ignoring ...\nopid: %d\n", client_input.op);
			
			if(sendto(server_socket, &end, sizeof(char), 0, (struct sockaddr *)&client_address, sizeof(struct sockaddr_un)) < 0) fprintf(error_file, "failed to send message to client\n");
	}
	
	return 0;	// default behavior; everything is fine and the daemon can keep running
}

int main() {
	/* file i/o declarations */
	int module_fd_list[MAX_MODULES];
	module module_registry[MAX_MODULES];
	int i = 0;
        struct dirent *file;
        DIR *module_dir, *bin_dir;
        FILE *error_file = fopen("./error_file", "w");
	/* socket declarations */
	int server_socket;
	struct sockaddr_un server_addr, client_addr;
	
	int daemon_state = 0;	// for breaking out of main loop
	char *answer = malloc(128);	// for responding to client; gets passed to handle_input()

	/* become daemon */
	if(fork() > 0) exit(0);
	umask(0);
	if(setsid() < 0) exit(1);
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
		
	/* zero out arrays for ease of parsing later */
	memset(module_fd_list, 0, sizeof(int) * MAX_MODULES);
	memset(module_registry, 0, sizeof(module) * MAX_MODULES);

        /* open modules folder */
        if((module_dir = opendir("./modules")) < 0) {
                fprintf(error_file, "failed to open modules directory\n");
                exit(1);
        }

        /* cycle through module files and grab their file descriptors*/
        while((file = readdir(module_dir)) != NULL && i < MAX_MODULES) {
                if(strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0) continue; // we don't want to load directories
                module_fd_list[i] = openat(dirfd(module_dir), file->d_name, O_RDONLY);
                ++i;
        }

        closedir(module_dir);

	/* open module binaries folder */
        if((bin_dir = opendir("./bin")) < 0) {
                fprintf(error_file, "failed to open module binaries directory\n");
                exit(1);
        }

        /* fetch file descriptors and port designations for each module binary */
        for(int i = 0; module_fd_list[i] != 0; ++i) {
                FILE *tmp_file = fdopen(module_fd_list[i], "r");	// assign stream to file descriptor
                fscanf(tmp_file, "%*s\n%s\n%*s\n%d", module_registry[i].name, &module_registry[i].port);        // note that the port and name are set inline here, but extra work is done for the file descriptor
                if((module_registry[i].fd = openat(dirfd(bin_dir), module_registry[i].name, O_RDONLY)) < 0) {	// verify that O_RDONLY works with fexecve()
			fprintf(error_file, "bad module name or module executable does not exist: %s\n", module_registry[i].name);
			exit(1);
		}

                fclose(tmp_file);       // clean up file descriptors as we go
        }

        closedir(bin_dir);
        
        /* specify that each module initializes in an OFF state */
        for(int i = 0; module_registry[i].fd != 0; ++i) {
		module_registry[i].state = false;
        }

	/* attempt to unlink any file residing in SOCK_PATH */
	if(unlink(SOCK_PATH) < 0 && errno != ENOENT) {
		fprintf(error_file, "failed to remove file at %s\n", SOCK_PATH);
		perror("guru meditation");
		exit(1);
	}

	/* initialize socket and addressing information */
	if((server_socket = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {	// we're going datagram for this one ;)
		fprintf(error_file, "failed to create unix domain socket\n");
		exit(1);
	}
	memset(&server_addr, 0, sizeof(struct sockaddr_un));
	server_addr.sun_family = AF_UNIX;
	snprintf(server_addr.sun_path, sizeof server_addr.sun_path, "%s", SOCK_PATH);	// extra-super-double safe string copying

	if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un)) < 0) {
		fprintf(error_file, "failed to bind server socket\n");
		perror("guru meditation");
		exit(1);
	}
	
	/* initialize client socket addressing information */
	memset(&client_addr, 0, sizeof(struct sockaddr_un));
	client_addr.sun_family = AF_UNIX;
	snprintf(client_addr.sun_path, sizeof client_addr.sun_path, "%s", CSOCK_PATH);

	/* main loop - wait for input */
	while(daemon_state == 0) daemon_state = handle_input(server_socket, client_addr, module_registry, answer, error_file);
	
	/* shut down all child processes */
	for(int i = 0; module_registry[i].fd != 0; ++i) {
		if(module_registry[i].state == true) {
			if(kill(module_registry[i].pid, SIGTERM) < 0) {
				fprintf(error_file, "failed to kill a child process\n");
				exit(1);
			}

			module_registry[i].state = false;
		}

	}

	
	/*  close all fds */
	for(int i = 0; module_registry[i].fd != 0; ++i) close(module_registry[i].fd);
	
	free(answer);
	fclose(error_file);
	exit(0);
}
