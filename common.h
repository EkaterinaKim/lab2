#pragma once
#ifndef LAB2FORKUDP_COMMON_H
#define LAB2FORKUDP_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>

// Defines
#define MAX_DATA_UDP 65536
enum LOG_TYPE {
    LOG_ONLY,
    LOG_n_STD
};
enum DATA_TYPE {
    FILE_STORAGE,
    LIST_STORAGE
};

typedef struct {
    enum DATA_TYPE type;
    char *name;
    void *src;
    int maxindex;
} STORAGE;
// Statics
FILE *logger;
// Export functions
void cool_log(enum LOG_TYPE, char*,...);

int cool_create_storage(STORAGE *);
int cool_write(STORAGE *, int, void*);
int cool_read(STORAGE *, int, void*);
int cool_drop_storage(STORAGE *);

#endif
