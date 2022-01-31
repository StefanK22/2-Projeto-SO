#include "tecnicofs_client_api.h"

int session;
int server;
char const *client_pipe_name;
int client;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {

    client_pipe_name = client_pipe_path;
    server = open(server_pipe_path, O_WRONLY);
    if (server == -1)
        return -1;

    if (unlink(client_pipe_path) != 0 && errno != ENOENT)
        return -1;

    if (mkfifo(client_pipe_path, 0777) < 0)
        return -1;
    
    char op = (char) TFS_OP_CODE_MOUNT;
    ssize_t ret = write(server, &op, sizeof(op));
    if (ret != sizeof(op))
        return -1;

    ret = write(server, client_pipe_path, sizeof(client_pipe_path));
    if (ret != sizeof(client_pipe_path))
        return -1;

    client = open(client_pipe_path, O_RDONLY);
    if (client == -1)
        return -1;

    ret = read(client, &session, sizeof(session));
    if (ret != sizeof(session))
        return -1;

    printf("entrou com o session id: %d\n", session);
    
    return 0;
}

int tfs_unmount() {

    char op = (char) TFS_OP_CODE_UNMOUNT;
    ssize_t ret = write(server, &op, sizeof(op));
    if (ret != sizeof(op))
        return -1;

    ret = write(server, &session, sizeof(session));
    if (ret != sizeof(session))
        return -1;

    close(server);
    close(client);

    if (unlink(client_pipe_name) != 0 && errno != ENOENT)
        return -1;

    printf("Saiu com session_id %d\n", session);

    return 0;
}

int tfs_open(char const *name, int flags) {

    char op = (char) TFS_OP_CODE_OPEN;
    ssize_t ret = write(server, &op, sizeof(op));
    if (ret != sizeof(op))
        return -1;
    
    ret = write(server, &session, sizeof(session));
    if (ret != sizeof(session))
        return -1;

    char path_name[40];
    memcpy(path_name, name, strlen(name));
    for (int i = (int) strlen(name); i < sizeof(path_name); i++){
        path_name[i] = '\0';
    }
    ret = write(server, path_name, sizeof(path_name));
    if (ret != sizeof(path_name))
        return -1;

    ret = write(server, &flags, sizeof(flags));
    if (ret != sizeof(flags))
        return -1;
    
    int fhandle;
    ret = read(client, &fhandle, sizeof(fhandle));
    if (ret != sizeof(fhandle))
        return -1;
    
    return fhandle;
}

int tfs_close(int fhandle) {
    char op = (char) TFS_OP_CODE_CLOSE;
    ssize_t ret = write(server, &op, sizeof(op));
    if (ret != sizeof(op))
        return -1;
    
    ret = write(server, &session, sizeof(session));
    if (ret != sizeof(session))
        return -1;

    ret = write(server, &fhandle, sizeof(fhandle));
    if (ret != sizeof(fhandle))
        return -1;
    
    int return_value;
    ret = read(client, &return_value, sizeof(return_value));
    if (ret == -1)
        return -1;
    
    return return_value;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    char op = (char) TFS_OP_CODE_WRITE;
    ssize_t ret = write(server, &op, sizeof(op));
    if (ret != sizeof(op))
        return -1;
    
    ret = write(server, &session, sizeof(session));
    if (ret != sizeof(session))
        return -1;

    ret = write(server, &fhandle, sizeof(fhandle));
    if (ret != sizeof(fhandle))
        return -1;

    ret = write(server, &len, sizeof(len));
    if (ret != sizeof(len))
        return -1;
    
    ret = write(server, buffer, len);
    if (ret != len)
        return -1;
    
    int return_value;
    ret = read(client, &return_value, sizeof(return_value));
    if (ret == -1 || return_value != len)
        return -1;

    return return_value;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    char op = (char) TFS_OP_CODE_READ;
    ssize_t ret = write(server, &op, sizeof(op));
    if (ret != sizeof(op))
        return -1;
    
    ret = write(server, &session, sizeof(session));
    if (ret != sizeof(session))
        return -1;

    ret = write(server, &fhandle, sizeof(fhandle));
    if (ret != sizeof(fhandle))
        return -1;

    ret = write(server, &len, sizeof(len));
    if (ret != sizeof(len))
        return -1;

    int bytes_read;
    ret = read(client, &bytes_read, sizeof(bytes_read));
    if (ret != sizeof(bytes_read))
        return -1;
    
    ret = read(client, buffer, len);
    if (ret != len)
        return -1;

    return bytes_read;
    
    
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
