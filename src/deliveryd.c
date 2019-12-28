#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/un.h>
#include <sys/socket.h>

#define MAX_MODULES 64
#define MODULE_NAME_LENGTH 16
#define SOCK_PATH "/tmp/deliveryd.sock"	// technically insecure, can be changed or secured by implementation

/* structure for holding module information */
typedef struct module {
	char name[MODULE_NAME_LENGTH];
	int fd;
	int port;
	int pid;
	bool state;
} module;

/* enumerated list of possible operations to be passed to the daemon */
typedef enum {
	start = 0,
	startall = 1,
	stop = 2,
	stoppid = 3,
	stopall = 4,
	restart = 5,
	restartpid = 6,
	shutdown = 7,
	info = 8,
	infopid = 9,
	status = 10
} operator;

/* structure for holding input information */
typedef struct input {
	operator op;
	char arg[MODULE_NAME_LENGTH];
} input;

/* start a module given its file descriptor, and return the resulting process' pid */
int start_module(int fd) {
	int pid = fork();
	if(pid == 0) {
		fexecve(fd);
		/* successful call never returns ... */
		fprintf(stderr, "call to fexecve returned, meaning that something went horribly wrong\n");
		exit(1);
	}
	
	return pid;
}

/* receive data from the client and return a structure containing operation information and argument */ 
void handle_input(int server_socket, module *module_registry) {
	input client_input;
	
	if(recvfrom(server_socket, client_input, sizeof(input), 0, NULL, NULL) < 0) {
		fprintf(stderr, "failed to receive from datagram socket\n");
		exit(1);
	}
	
	/* switch case for input handling - refer to operator enum type */
	switch(client_input.op) {
		case 0:		// start module by name
			for(int i = 0; module_registry[i].fd != 0; ++i) {
				if(strcmp(module_registry[i].name, client_input.arg) == 0) {
					if(module_registry[i].state == false) {
						module_registry[i].pid = start_module(module_registry[i].fd);
						module_registry[i].state = true;
					}
					
					else break;
				}
			}
			
			break;
		
		case 1:		// start all modules
			for(int i = 0; module_registry[i].fd != 0; ++i) {
				if(module_registry[i].state == false) { 
					module_registry[i].pid = start_module(module_registry[i].fd);
					module_registry[i].state = true;
				}					 
			}
			
			break;
		
		case 2:		// stop module by name
		
		case 3:		// stop module by pid
		
		case 4:		// stop all modules
		
		case 5: 	// restart module by name
		
		case 6: 	// restart module by pid
		
		case 7: 	// shutdown the daemon
		
		case 8:		// return info on a module by name
		
		case 9:		// return info on a module by pid
		
		case 10:	// return a general status for the daemon
			/* systematically print module names, ports, and states/pids */
			for(int i = 0; module_registry[i].fd != 0; ++i) {
				printf("%-16s %8d active: %d\n", module_registry[i].name, module_registry[i].port, module_registry[i].state == true ? module_registry[i].pid : false);
			}
			
			break;
		
		default:	// something got messed up big time client-side
			fprintf(stderr, "unknown operator passed by client - ignoring ...\nopid: %d\n", client_input.op);
			break;
	}
}

int main() {
	/* file i/o declarations */
	int module_fd_list[MAX_MODULES];
	module module_registry[MAX_MODULES];
	int i = 0;
        struct dirent *file;
        DIR *module_dir, *bin_dir;
	/* socket declarations */
	int server_socket, client_socket;
	struct sockaddr_un server_addr;

	/* zero out arrays for ease of parsing later */
	memset(module_fd_list, 0, sizeof(int) * MAX_MODULES);
	memset(module_registry, 0, sizeof(module) * MAX_MODULES);

        /* open modules folder */
        if((module_dir = opendir("./modules")) < 0) {
                fprintf(stderr, "failed to open modules directory\n");
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
                fprintf(stderr, "failed to open module binaries directory\n");
                exit(1);
        }

        /* fetch file descriptors and port designations for each module binary */
        for(int i = 0; module_fd_list[i] != 0; ++i) {
                FILE *tmp_file = fdopen(module_fd_list[i], "r");	// assign stream to file descriptor
                fscanf(tmp_file, "%*s\n%s\n%*s\n%d", module_registry[i].name, &module_registry[i].port);        // note that the port and name are set inline here, but extra work is done for the file descriptor
                if((module_registry[i].fd = openat(dirfd(bin_dir), module_registry[i].name, O_RDONLY)) < 0) {	// verify that O_RDONLY works with fexecve()
			fprintf(stderr, "bad module name or module executable does not exist: %s\n", module_registry[i].name);
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
	if(remove(SOCK_PATH) < 0 && errno != ENOENT) {
		fprintf(stderr, "failed to remove file at %s", SOCK_PATH);
		exit(1);
	}

	/* initialize socket and addressing information */
	if((server_socket = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {	// we're going datagram for this one ;)
		fprintf(stderr, "failed to create unix domain socket\n");
		exit(1);
	}
	memset(&server_addr, 0, sizeof(struct sockaddr_un));
	server_addr.sun_family = AF_UNIX;
	snprintf(server_addr.sun_path, sizeof server_addr.sun_path, "%s", SOCK_PATH);	// extra-super-double safe string copying

	if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un)) < 0) {
		fprintf(stderr, "failed to bind server socket\n");
		exit(1);
	}

	/* main loop - wait for input */
	for(;;) handle_input(server_socket);
	
	
	/* print module info for debug purposes then close all fds */
	for(int i = 0; module_registry[i].fd != 0; ++i) {
		printf("%s:%d\n", module_registry[i].name, module_registry[i].port);
		close(module_registry[i].fd);
	}

}
