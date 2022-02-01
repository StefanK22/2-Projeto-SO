#include "tecnicofs_client_api.h"
#include <pthread.h>
int server;
char const *client_pipe_name;
int client;

request req;


int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {

    pthread_cond_init(&req.prod, NULL);
    pthread_cond_init(&req.cons, NULL);
    pthread_mutex_init(&req.mutex, NULL);
    req.requested = 0;
    client_pipe_name = client_pipe_path;
    server = open(server_pipe_path, O_WRONLY);
    if (server == -1)
        return -1;

    if (unlink(client_pipe_path) != 0 && errno != ENOENT)
        return -1;

    if (mkfifo(client_pipe_path, 0777) < 0)
        return -1;

    /*
    char op = (char) TFS_OP_CODE_MOUNT;
    ssize_t ret = write(server, &op, sizeof(op));
    if (ret != sizeof(op))
        return -1;

    ret = write(server, client_pipe_path, sizeof(client_pipe_path));
    if (ret != sizeof(client_pipe_path))
        return -1;
    */

    req.op_code = (char) TFS_OP_CODE_MOUNT;
    req.session_id = -1;
    memcpy(req.name, client_pipe_path, strlen(client_pipe_path));
    ssize_t ret = write(server, &req, sizeof(req));
    if (ret != sizeof(req))
        return -1;
    
    client = open(client_pipe_path, O_RDONLY);
    if (client == -1)
        return -1;

    ret = read(client, &req.session_id, sizeof(req.session_id));
    if (ret != sizeof(req.session_id))
        return -1;

    printf("entrou com o session id: %d\n", req.session_id);
    
    return 0;
}

int tfs_unmount() {

    req.op_code = (char) TFS_OP_CODE_UNMOUNT;
    ssize_t ret = write(server, &req, sizeof(req));
    if (ret != sizeof(req))
        return -1;

    close(server);
    close(client);

    if (unlink(client_pipe_name) != 0 && errno != ENOENT)
        return -1;

    printf("Saiu com session_id %d\n", req.session_id);

    return 0;
}

int tfs_open(char const *name, int flags) {

    req.op_code = (char) TFS_OP_CODE_OPEN;
    
    char path_name[40];
    memcpy(path_name, name, strlen(name));
    for (int i = (int) strlen(name); i < sizeof(path_name); i++){
        path_name[i] = '\0';
    }
    memcpy(req.name, path_name, sizeof(path_name));
    req.flags = flags;
    
    ssize_t ret = write(server, &req, sizeof(req));
    if (ret != sizeof(req))
        return -1;

    int fhandle;
    ret = read(client, &fhandle, sizeof(fhandle));
    if (ret != sizeof(fhandle))
        return -1;
    
    return fhandle;
}

int tfs_close(int fhandle) {
    
    req.op_code = (char) TFS_OP_CODE_CLOSE;
    req.fhandle = fhandle;

    ssize_t ret = write(server, &req, sizeof(req));
    if (ret != sizeof(req))
        return -1;
    
    int return_value;
    ret = read(client, &return_value, sizeof(return_value));
    if (ret == -1)
        return -1;
    
    return return_value;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {

    req.op_code = (char) TFS_OP_CODE_WRITE;
    req.fhandle = fhandle;
    req.len = len;    
    memcpy(req.buffer, buffer, sizeof(*buffer));

    ssize_t ret = write(server, &req, sizeof(req));
    if (ret != sizeof(req))
        return -1;
    
    int return_value;
    ret = read(client, &return_value, sizeof(return_value));
    if (ret == -1 || return_value != len)
        return -1;

    return return_value;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    req.op_code = (char) TFS_OP_CODE_READ;
    req.fhandle = fhandle;
    req.len = len;

    ssize_t ret = write(server, &req, sizeof(req));
    if (ret != sizeof(req))
        return -1;

    int bytes_read;
    ret = read(client, &bytes_read, sizeof(bytes_read));
    if (ret != sizeof(bytes_read))
        return -1;
    
    ret = read(client, buffer, (size_t) bytes_read);
    if (ret != bytes_read)
        return -1;

    return bytes_read;
    
    
}

int tfs_shutdown_after_all_closed() {
    req.op_code = (char) TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;

    ssize_t ret = write(server, &req, sizeof(req));
    if (ret != sizeof(req))
        return -1;
    
    int return_value;
    ret = read(client, &return_value, sizeof(return_value));
    if (ret == -1 || return_value == -1)
        return -1;
    
    return 0;
}
