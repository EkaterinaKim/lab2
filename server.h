#pragma once
#ifndef LAB2FORKUDP_SERVER_H
#define LAB2FORKUDP_SERVER_H

// System libs
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Export functions
void daemonize(void);
void server(char*, int, int, int);
void client(char*, int, char*);


#endif
