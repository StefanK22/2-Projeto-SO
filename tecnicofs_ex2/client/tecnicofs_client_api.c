#include "tecnicofs_client_api.h"

int session;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {

    int server = open(server_pipe_path, O_WRONLY);

    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", client_pipe_path,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (mkfifo(client_pipe_path, 0640) != 0)
        return -1;
    
    size_t ret = write(server, client_pipe_path, sizeof(client_pipe_path));
    if (ret != sizeof(client_pipe_path))
        return -1;

    int client = open(client_pipe_path, O_RDONLY);
    char session_id[40];
    read(client, session_id, sizeof(session_id));
    printf("%s\n", session_id);
    
    return 0;
}

int tfs_unmount() {
    /* TODO: Implement this */
    return -1;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    return -1;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
