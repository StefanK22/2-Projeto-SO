#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <pthread.h>
#define BLOCK_SIZE (1024)
#define NAME_SIZE (40)
#define MAX_SESSIONS (10)

/* tfs_open flags */
enum {
    TFS_O_CREAT = 0b001,
    TFS_O_TRUNC = 0b010,
    TFS_O_APPEND = 0b100,
};

/* operation codes (for client-server requests) */
enum {
    TFS_OP_CODE_MOUNT = 1,
    TFS_OP_CODE_UNMOUNT = 2,
    TFS_OP_CODE_OPEN = 3,
    TFS_OP_CODE_CLOSE = 4,
    TFS_OP_CODE_WRITE = 5,
    TFS_OP_CODE_READ = 6,
    TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED = 7
};

typedef struct {
    char op_code;
    int session_id, flags, fhandle;
    size_t len;
    char name[NAME_SIZE];
    char buffer[BLOCK_SIZE];
    pthread_cond_t cons;
    pthread_cond_t prod;
    pthread_mutex_t mutex;
    int requested;
} request;

#endif /* COMMON_H */