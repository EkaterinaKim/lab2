#include "common.h"
#include "server.h"

char *current_program;
/* options */
struct globalArgs_t {
#ifdef SERVER
    int daemon; // run as daemon
    int version; // show version
    int delay_sleep; // simulate the work
    int common_storage; // special option: dont use common storage
    char *logfile_name;
#else
    char *message; // message of client
#endif
    char *ipaddr;
    int ipport;
} globalArgs;

/* parser of options */
void parser(int, char**);
/* show help and errors*/
void display_usage( char* );

int main(int argc, char **argv) {
    parser(argc, argv); // parse option firstly
#ifdef SERVER
    if (globalArgs.daemon) daemonize(); // run as daemon if need
    if (!(logger = fopen(globalArgs.logfile_name, "a"))) {
        display_usage("<MAIN> Opening log");
    }
    cool_log(LOG_n_STD, "Main part started\n");

    server(globalArgs.ipaddr, globalArgs.ipport, globalArgs.delay_sleep, globalArgs.common_storage);
    cool_log(LOG_n_STD, "Main part finished\n");
    fclose(logger);
#else
    client(globalArgs.ipaddr, globalArgs.ipport, globalArgs.message);
#endif
    return 0;
}

void display_usage( char* error ) {
    if (error) {
        perror(error);
    }
    char *format = "Usage: %s [options]\n"
                   "\tOptions:\n"
                   "\t\t-h show usage\n"
                   #ifdef SERVER
                   "\t\t-v show version\n"
                   "\t\t-c force use personal ip storages\n"
                   "\t\t-d run as daemon\n"
                   "\t\t-w N emulate work: sleep N seconds (default 0, option may be passed through environment variable L2WAIT)\n"
                   "\t\t-l LOGFILE specify logfile_name (default '/tmp/lab2.log', option may be passed through environment variable L2LOGFILE)\n"
                   #else
                   "\t\t-m MESSAGE message to sent to server"
                   #endif
                   "\t\t-a IPADRESS determine ip address to listen (option may be passed through environment variable L2ADDR)\n"
                   "\t\t-p IPPORT determine port to listen (option may be passed through environment variable L2PORT)\n";

    printf(format, current_program);
    exit( error ? EXIT_FAILURE : EXIT_SUCCESS );
}
void parser( int argc, char **argv ) {
    char *tmp;
#ifdef SERVER
    globalArgs.daemon = 0;
    globalArgs.version = 0;
    globalArgs.delay_sleep = 0;
    globalArgs.common_storage = 1;
    globalArgs.logfile_name = "/tmp/lab2.log";

    tmp = getenv("L2WAIT");
    if (tmp) sscanf(tmp, "%u", &globalArgs.delay_sleep);
    tmp = getenv("L2LOGFILE");
    if (tmp) globalArgs.logfile_name = strdup(tmp);
#else
    globalArgs.message = NULL;
#endif
    globalArgs.ipaddr = NULL;
    globalArgs.ipport = 0;
    current_program = argv[0];

    tmp = getenv("L2ADDR");
    if (tmp) globalArgs.ipaddr = strdup(tmp);
    tmp = getenv("L2PORT");
    if (tmp) sscanf(tmp, "%u", &globalArgs.ipport);

    // Parse options
#ifdef SERVER
    const char *optString = "w:dl:a:p:vch?";
#else
    const char *optString = "m:a:p:h?";
#endif
    int opt = 0;
    do {
        switch( opt ) {
#ifdef SERVER
            case 'v':
                globalArgs.version = 1;
                printf("%s %s\n", current_program, "version 1.0");
                exit(EXIT_SUCCESS);
                break;
            case 'd':
                globalArgs.daemon = 1;
                break;

            case 'c':
                globalArgs.common_storage = 0;
                break;

            case 'w':
                sscanf(optarg, "%u", &globalArgs.delay_sleep);
                break;
            case 'l':
                globalArgs.logfile_name = optarg;
                break;
#else
            case 'm':
                globalArgs.message = optarg;
                break;
#endif
            case 'a':
                globalArgs.ipaddr = optarg;
                break;
            case 'p':
                sscanf(optarg, "%u", &globalArgs.ipport);
                break;
            case 'h':
                display_usage(0);
                break;
            case '?':
                break;
        }
        opt = getopt( argc, argv, optString );
    } while( opt != -1 );

    if (!globalArgs.ipaddr || !globalArgs.ipport || !inet_aton(globalArgs.ipaddr,0))
        display_usage("Address and port is required options and must be valid");

}
