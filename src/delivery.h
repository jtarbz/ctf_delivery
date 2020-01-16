#define SOCK_PATH "/tmp/deliveryd.sock"	// technically insecure, can be changed or secured by implementation
#define CSOCK_PATH "/tmp/delivery_client.sock"
#define MODULE_NAME_LENGTH 16

/* enumerated list of possible operations to be passed to the daemon */
typedef enum {
        start = 0,
        startall = 1,
        stop = 2,
        stoppid = 3,
        stopall = 4,
        restart = 5,
        restartpid = 6,
        shutdown_proc = 7,
        status = 8
} operator;

/* structure for holding input information */
typedef struct input {
    operator op;
    char arg[MODULE_NAME_LENGTH];
} input;