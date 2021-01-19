#include "common.h"
#include <sys/file.h>

static char *time_format = "[%d.%m.%Y %H:%M:%S] >>> ";
static int time_format_len = 27;

void cool_log(enum LOG_TYPE type, char* format,...) {
    if (logger) {
        /* create time string */
        time_t rawtime;
        struct tm * timeinfo;
        char *time_string = (char *)malloc((time_format_len+1) * sizeof(char));
        if (!time_string) {
            perror("<main> cool_log");
            return;
        }
        time (&rawtime);
        timeinfo = localtime (&rawtime);
        strftime (time_string,time_format_len,time_format,timeinfo);
        time_string[time_format_len+1] = '\0';
        /* and print it */
        fprintf(logger, "%s ", time_string);
        free(time_string);

        if (!format) { // this mean that error occurred
            fprintf(logger, "ERROR: %s\n", strerror(errno));
        } else { // write all we need to logger
            va_list ptr;
            va_start(ptr, format);
            vfprintf(logger, format, ptr);
            va_end(ptr);
            fflush(logger);
        }
    }
    if (type == LOG_n_STD) { // write the same to stdout
        if (!format) {
            fprintf(stderr, "ERROR: %s\n", strerror(errno));
        } else {
            va_list ptr;
            va_start(ptr, format);
            vfprintf(stdout, format, ptr);
            va_end(ptr);
            fflush(stdout);
        }
    }

}

typedef struct LinkedList{
    void *data;
    struct LinkedList *next;
} node;
node *createNode(){
    node *temp;
    temp = (node*)malloc(sizeof(node));
    temp->data = malloc(sizeof(int));
    temp->next = NULL;
    return temp;
}
void dropNode(node* cur){
    if (cur->next) {
        dropNode(cur->next);
    }
    cur->next = NULL;
    free(cur->data);
    free(cur);
}

static char* prefix_name = "storage_";
/* if we need file storage instead of list */
static char* file_storage_dir = "/tmp/";
/* try lock file */
int do_lock(int fd)
{
    int ret = -1;
    while (1)
    {
        ret = flock(fd, LOCK_EX | LOCK_NB);
        if (ret == 0)
        {
            break;
        }
        usleep((rand() % 10) * 1000);
    }
    return ret;
}

/* unlock file */
int do_unlock(int fd)
{
    return flock(fd, LOCK_UN);
}

int cool_create_storage(STORAGE * storage) {
    if (storage->type == FILE_STORAGE) {
        /* open with create or append and read file */
        char *full_name = calloc(strlen(file_storage_dir) + strlen(prefix_name) + strlen(storage->name) + 1, sizeof(char));
        sprintf(full_name, "%s%s%s", file_storage_dir, prefix_name, storage->name);
        storage->src = fopen(full_name, "r+b");
        if (storage->src == 0) storage->src = fopen(full_name, "w+b");
        fseek(storage->src, 0L, SEEK_END);
        storage->maxindex = ftell(storage->src) / sizeof(int);
        free(full_name);
        return 0;
    } else if (storage->type == LIST_STORAGE) {
        /* create head of list */
        storage->src = createNode();
        storage->maxindex = 0;
        return 0;
    } else {
        return 1;
    }
}

int cool_write(STORAGE * storage, int index, void* data) {
    if (storage->type == FILE_STORAGE) {
        /* lock file and write to the end */
        do_lock(fileno(storage->src));
        if (index != -1) {
            fseek(storage->src, index*sizeof(int), SEEK_SET);
        } else {
            fseek(storage->src, 0, SEEK_END);
        }
        fwrite(data, sizeof(int), 1, storage->src);
        storage->maxindex++;
        do_unlock(fileno(storage->src));
        return 0;
    } else if (storage->type == LIST_STORAGE) {
        /* add element to the end of list */
        node *tmp = storage->src;
        while (tmp->next) tmp = tmp->next;
        tmp->next = createNode();
        memcpy(tmp->data, data, sizeof(int));
        storage->maxindex++;
        return 0;
    } else {
        return 1;
    }
}
int cool_read(STORAGE * storage, int index, void* data) {
    if (storage->type == FILE_STORAGE) {
        /* read needed element in file */
        fseek(storage->src, index*sizeof(int), SEEK_SET);
        return 1 != fread(data, sizeof(int), 1, storage->src);
    } else if (storage->type == LIST_STORAGE) {
        /* find needed element in list */
        if (index >= storage->maxindex) return 1;
        node *tmp = storage->src;
        while (index && tmp->next != NULL) {
            tmp = tmp->next;
            index--;
        }
        memcpy(data, tmp->data, sizeof(int));
        return 0;
    } else {
        return 1;
    }
}

int cool_drop_storage(STORAGE * storage) {
    if (storage->type == FILE_STORAGE) {
        /* close file */
        fclose(storage->src);
        return 0;
    } else if (storage->type == LIST_STORAGE) {
        /* delete list */
        dropNode(storage->src);
        return 0;
    } else {
        return 1;
    }
}
