#include "server.h"
#include "common.h"
#include "sys/stat.h"
#include <fcntl.h>
#include <wait.h>
enum TYPES_CONNECTION {SERV, CLI};

typedef struct worker_data { // for sending and receiving
    char *data;
    int datalen;
    struct sockaddr_in *address;
    socklen_t len;
    int delay_sleep;
    int common_storage;
} worker_data;

int succeed, errors; // for statistics

// create socket
int create_socket(char *ip, int port,  struct sockaddr_in * addr, enum TYPES_CONNECTION type) {
    int sockfd = 0;
    if ((sockfd=socket(AF_INET,SOCK_DGRAM,0)) < 0) {
        cool_log(LOG_n_STD, 0);
        exit(EXIT_FAILURE);
    }
    addr->sin_family=AF_INET;
    addr->sin_addr.s_addr=inet_addr(ip);
    addr->sin_port=htons(port);
    /* to reuse socket few times */
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

    if (type == SERV) {
        /* non blocking socket */
        fcntl(sockfd, F_SETFL, O_NONBLOCK);

        if(bind(sockfd, (const struct sockaddr *)addr, sizeof(*addr)) < 0) {
            cool_log(LOG_n_STD, 0);
            exit(EXIT_FAILURE);
        }
    }

    return sockfd;
}

// handlers
void term_handler(int);
void quit_handler(int);
void usr1_handler(int);
void set_sighandlers() {
    struct sigaction term_act, quit_act, usr1_act;
    memset(&term_act, 0, sizeof(term_act));
    term_act.sa_handler = term_handler;
    memset(&quit_act, 0, sizeof(quit_act));
    quit_act.sa_handler = quit_handler;
    memset(&usr1_act, 0, sizeof(usr1_act));
    usr1_act.sa_handler = usr1_handler;
    sigaction(SIGTERM, &term_act, 0);
    sigaction(SIGQUIT, &quit_act, 0);
    sigaction(SIGINT, &quit_act, 0);
    sigaction(SIGUSR1, &usr1_act, 0);
}

// for receive from everybody or from specified server
int get_message(int sock, void **arg) {
    char tmpbuf[MAX_DATA_UDP+1] = {0};
    struct sockaddr_in tmpaddr;
    int tmplen = sizeof(tmpaddr);
    int tmpdatalen;
    worker_data *args;
    if (*arg && (args = *arg)) {
        /* client type */
        tmpdatalen = recvfrom(sock, tmpbuf, MAX_DATA_UDP, MSG_WAITALL, (struct sockaddr *) args->address, &args->len);
    } else {
        /* server type */
        tmpdatalen = recvfrom(sock, tmpbuf, MAX_DATA_UDP, MSG_WAITALL, (struct sockaddr *) &tmpaddr, &tmplen);
    }
    if (tmpdatalen > 0) {
        if (*arg == NULL) {
            /* server type */
            *arg = calloc(1, sizeof(worker_data));
            args = *arg;
            args->len = tmplen;
            args->address = calloc(1, sizeof(struct sockaddr_in));
            memcpy(args->address, &tmpaddr, tmplen);
        }
        args->data = calloc(tmpdatalen + 1, sizeof(char));
        memcpy(args->data, tmpbuf, tmpdatalen);
        args->datalen = tmpdatalen;
        if (args->address) {
            char host[20];
            inet_ntop(AF_INET,&args->address->sin_addr,host,sizeof(host));
            cool_log(LOG_n_STD, "Connection from %s:%d\n", host, ntohs(args->address->sin_port));
            cool_log(LOG_n_STD, "RECV from %s:%d\n", host, ntohs(args->address->sin_port));
        }
        return args->datalen;
    } else return -1;
}

// for sending
int send_message(int sock, void **arg) {
    worker_data *args = *arg;
    if (args->datalen) {
        char host[20];
        inet_ntop(AF_INET, &args->address->sin_addr, host, sizeof(host));
        cool_log(LOG_n_STD, "SEND to %s:%d\n", host, ntohs(args->address->sin_port));
        return sendto(sock, args->data, strlen(args->data), 0, (const struct sockaddr *) args->address, args->len);
    }
    return 1;
}

int close_conn(int sock, void **arg) {
    worker_data *args = *arg;
    char host[20];
    inet_ntop(AF_INET, &args->address->sin_addr, host, sizeof(host));
    cool_log(LOG_n_STD, "Close connection with %s:%d\n", host, ntohs(args->address->sin_port));
    free(args->data);
    free(args->address);
    free(args);
    return 0;
}


void update_stats(int stat) {
    /* for process read exit status */
    stat = -1;
    if (waitpid(-1, &stat, WNOHANG) > 0 && stat >= 0) {
        stat ? succeed++ : errors++;
    }
}

// variant based part
int program(char *request, char* response, STORAGE* storage) {
    char *tmp = strtok(request, " ");
    if (tmp && !strncmp(tmp, "PUT", 3)) {
        tmp = strtok(NULL, " ");
        int num;
        if (tmp && sscanf(tmp,"%d", &num)) {
            int index = 0;
            if (cool_read(storage, 0, &index)) {
                index=0;
                cool_write(storage, 0, &index);
            }
            cool_write(storage, -1, &num);
            strcpy(response, "OK\n");
            return 1;
        } else {
            strcpy(response, "ERROR 1\n");
        }
    } else if (tmp && !strncmp(tmp, "GET", 3)) {
        if (storage->maxindex > 0) {
            int index = 0;
            if (!cool_read(storage, 0, &index)) {
                int num;
                index++;
                if (!cool_read(storage, index, &num)) {
                    cool_write(storage, 0, &index);
                    sprintf(response, "%d\n", num);
                    return 1;
                } else {
                    strcpy(response, "ERROR 4\n");
                }
            } else {
                strcpy(response, "ERROR 6\n");
            }
        } else {
            strcpy(response, "ERROR 5\n");
        }
    } else {
        strcpy(response, "ERROR 2\n");
    }
    return 0;
}

/* for create worker */
typedef struct server_ars {
    worker_data *args;
    int c_socket;
} server_ars;

// main for worker
void *process_program(void *arg) {
    server_ars *args = arg;
    STORAGE tmp;
    tmp.type = FILE_STORAGE;
    char host[20];
    if (args->args->common_storage) {
        strncpy(host, "common", 7);
    } else {
        inet_ntop(AF_INET, &args->args->address->sin_addr, host, sizeof(host));
    }
    tmp.name = host;
    cool_create_storage(&tmp);

    char *response = calloc(MAX_DATA_UDP+1, sizeof(char));
    int success = program(args->args->data, response, &tmp);

    cool_drop_storage(&tmp);

    free(args->args->data);
    args->args->data = response;
    args->args->datalen = strlen(response);
    if (args->args->delay_sleep) sleep(args->args->delay_sleep);
    if (send_message(args->c_socket, &args->args) == -1 ) cool_log(LOG_n_STD, 0);
    _exit(success);
}


void *create_worker(int c_sock, void *worker_data) {
    pid_t pid = fork();
    if (pid < 0) {
        cool_log(LOG_n_STD, 0);
    } else if (pid == 0) {
        server_ars *args = calloc(1, sizeof(server_ars));
        args->c_socket = c_sock;
        args->args = worker_data;
        process_program(args);
    }
}

int killall = 0; // to stop cycle
time_t begin; // time of begining
void server(char* ip, int port, int delay_sleep, int common_storage) {
    begin = time(NULL);
    set_sighandlers();
    struct sockaddr_in server_addr;
    int sockfd = create_socket(ip, port, &server_addr, SERV);
    while (!killall) {
        int c_sockfd;
        worker_data *arg = NULL;
        if ((c_sockfd = sockfd) > 0 && get_message(c_sockfd, &arg) > 0) {
            arg->delay_sleep = delay_sleep;
            arg->common_storage = common_storage;
            create_worker(c_sockfd, arg);
            close_conn(c_sockfd, &arg);
        }
        update_stats(0);
    }
}

// client = send + recv
void client(char* ip, int port, char *message) {
    struct sockaddr_in *server_addr = calloc(1, sizeof(struct sockaddr_in));
    int sockfd = create_socket(ip, port, server_addr, CLI);
    int c_sockfd;
    worker_data *arg = calloc(1, sizeof(worker_data));
    if ((c_sockfd = sockfd) > 0) {
        arg->address = server_addr;
        arg->data = message;
        arg->datalen = strlen(message);
        arg->len = sizeof(*arg->address);
        send_message(c_sockfd, &arg);
        get_message(c_sockfd, &arg);
        printf("%s\n", arg->data);
        close_conn(c_sockfd, &arg);
    }
}

// MAN 7 daemon (similar)
void daemonize(void) {
    close(fileno(stdin));
    close(fileno(stdout));
    close(fileno(stderr));
    int fd[2];
    pipe(fd);
    char pid[sizeof(unsigned int)] = {0};
    pid_t pid_1 = fork();
    if (pid_1 < 0) {
        perror("Daemon fork 1");
        exit(EXIT_FAILURE);
    } else if (pid_1 == 0) {
        setsid();
        pid_t pid_2 = fork();
        if (pid_2 < 0) {
            perror("Daemon fork 2");
            exit(EXIT_FAILURE);
        } else if (pid_2 == 0) {
            FILE *ftmp = fopen("/dev/null", "rb");
            dup(fileno(ftmp));
            close(fileno(ftmp));
            ftmp = fopen("/dev/null", "wb");
            dup(fileno(ftmp));
            dup(fileno(ftmp));
            close(fileno(ftmp));
            umask(0);
            chdir("/");
            (*((unsigned int*)pid)) = getpid();
            write(fd[1], pid, sizeof(unsigned int));
            close(fd[0]);
            close(fd[1]);
        } else {
            exit(EXIT_SUCCESS);
        }
    } else {
        read(fd[0], pid, sizeof(unsigned int));
        exit((*((unsigned int*)pid)) ? EXIT_SUCCESS : EXIT_FAILURE);
    }
}
// stop creating new workers + wait for their end
void quit_handler(int sig) {
    killall = 1;
    cool_log(LOG_n_STD, "Cycle exited\n");
}

// quit + kill
void term_handler(int sig) {
    cool_log(LOG_n_STD,  "SIGTERM\n");
    quit_handler(sig);
    exit(EXIT_SUCCESS);
}

// seconds to str
void sec2time(int sec, char* buf) {
    int h, m, s;
    h = (sec/3600);

    m = (sec -(3600*h))/60;

    s = (sec -(3600*h)-(m*60));
    sprintf(buf,"%04d:%02d:%02d",h,m,s);

}

// show statistics
void usr1_handler(int sig) {
    time_t end = time(NULL);
    double time_spent = difftime(end, begin);
    char diff[12];
    sec2time(time_spent, diff);
    cool_log(LOG_n_STD, "Server works %s\n", diff);
    cool_log(LOG_n_STD, "STATISTIC: Errors: %d; Succeed: %d\n", errors, succeed);
}
