#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "delivery.h"

/* print help message and exit */
void print_help(void) {
    printf("usage:\tdelivery [option] [argument]\n \
    options:\n \
        start\t\tstart a module by name\n \
        startall\tstart all modules\n \
        stop\t\tstop a module by name\n \
        stoppid\tstop a module by pid\n \
        stopall\tstop all modules\n \
        restart\trestart a module by name\n \
        restartpid\trestart a module by pid\n \
        shutdown_proc\tshutdown the daemon\n \
        status\t\tdisplay a status of the daemon\n\n \
    modules should be in the standard form - see the README for help\n \
    created by jason walter\n");
    
    exit(0);
}

int main(int argc, char **argv) {
    input client_input;
    int client_socket, server_socket;
    struct sockaddr_un client_addr, server_addr;
    
    if(argc < 2) print_help();

    /* massive set of argument conditionals ;) */
    
    /* start module by name */
    if(strcmp(argv[1], "start") == 0) {
        client_input.op = start;
        snprintf(client_input.arg, MODULE_NAME_LENGTH, "%s", argv[2]);
    }
    /* start all modules */
    else if(strcmp(argv[1], "startall") == 0) client_input.op = startall;
    /* stop module by name */
    else if(strcmp(argv[1], "stop") == 0) {
        client_input.op = stop;
        snprintf(client_input.arg, MODULE_NAME_LENGTH, "%s", argv[2]);
    }
    /* stop module by pid */
    else if(strcmp(argv[1], "stoppid") == 0) {
        client_input.op = stoppid;
        snprintf(client_input.arg, 6, "%s", argv[2]);
    }
    /* stop all modules */
    else if(strcmp(argv[1], "stopall") == 0) client_input.op = stopall;
    /* restart module by name */
    else if(strcmp(argv[1], "restart") == 0) {
        client_input.op = restart;
        snprintf(client_input.arg, MODULE_NAME_LENGTH, "%s", argv[2]);
    }
    /* restart module by pid */
    else if(strcmp(argv[1], "restartpid") == 0) {
        client_input.op = restartpid;
        snprintf(client_input.arg, 6, "%s", argv[2]);
    }
    /* shutdown daemon */
    else if(strcmp(argv[1], "shutdown_proc") == 0) client_input.op = shutdown_proc;
    /* display daemon status */
    else if(strcmp(argv[1], "status") == 0) client_input.op = status;
    /* your input doesn't exist */
    else print_help();
    
    /* initialize client socket and addressing information */
    if((client_socket = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "failed to bind client socket\n");
        exit(1);
    }
    /* NOT NEEDED UNTIL FUNCTIONALITY IS EXPANDED
    
    memset(&client_addr, 0, sizeof(struct sockaddr_un));
    client_addr.sun_family = AF_UNIX;
    snprintf(client_addr.sun_path, sizeof client_addr.sun_path, "%s", CSOCK_PATH); */

    /* initialize server socket addressing information */
    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    snprintf(server_addr.sun_path, sizeof server_addr.sun_path, "%s", SOCK_PATH);

    if(sendto(client_socket, &client_input, sizeof(input), 0, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un)) < 0) {
        fprintf(stderr, "failed to send message to daemon\n");
        exit(1);
    }

    exit(0);
}