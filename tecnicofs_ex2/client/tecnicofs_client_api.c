#include "tecnicofs_client_api.h"
#include <pthread.h>
int server;
char client_pipe_name[NAME_SIZE];
int client;
int session_id;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {

    memcpy(client_pipe_name, client_pipe_path, NAME_SIZE);
    server = open(server_pipe_path, O_WRONLY);
    if (server == -1)
        return -1;

    if (unlink(client_pipe_path) != 0 && errno != ENOENT)
        return -1;

    if (mkfifo(client_pipe_path, 0777) < 0)
        return -1;

    request req;
    req.session_id = -1;
    req.op_code = TFS_OP_CODE_MOUNT;
    memcpy(req.name, client_pipe_path, strlen(client_pipe_path));
    ssize_t ret = write(server, &req, sizeof(req));
    if (ret != sizeof(req))
        return -1;
    
    client = open(client_pipe_path, O_RDONLY);
    if (client == -1)
        return -1;

    ret = read(client, &session_id, sizeof(session_id));
    if (ret != sizeof(session_id))
        return -1;

    if (session_id == -1){
        unlink(client_pipe_name);
        return -1;
    }

    printf("Entrou com session id %d\n", session_id);
    return 0;
}

int tfs_unmount() {

    request req;
    req.session_id = session_id;
    req.op_code = (char) TFS_OP_CODE_UNMOUNT;
    ssize_t ret = write(server, &req, sizeof(req));
    if (ret != sizeof(req))
        return -1;

    if (close(server) != 0)
        exit(EXIT_FAILURE);
    if (close(client) != 0)
        exit(EXIT_FAILURE);

    if (unlink(client_pipe_name) != 0 && errno != ENOENT)
        return -1;

    printf("saiu com session id %d\n", session_id);
    return 0;
}

int tfs_open(char const *name, int flags) {
    
    request req;
    req.session_id = session_id;
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
    
    request req;
    req.session_id = session_id;
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

    request req;
    req.session_id = session_id;
    req.op_code = (char) TFS_OP_CODE_WRITE;
    req.fhandle = fhandle;
    req.len = len;    
    memcpy(req.buffer, buffer, strlen(buffer));

    ssize_t ret = write(server, &req, sizeof(req));
    if (ret != sizeof(req))
        return -1;
    
    int return_value;
    ret = read(client, &return_value, sizeof(return_value));
    if (ret == -1)
        return -1;

    return return_value;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {

    request req;
    req.session_id = session_id;
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

    request req;
    req.session_id = session_id;
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
