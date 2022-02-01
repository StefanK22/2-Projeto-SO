#include "operations.h"

int sessions = 0;
int open_clients[10];
int server;
char* server_pipe_name;


int receive_requests();
int tfs_mount_server();
int tfs_unmount_server();
int tfs_open_server();
int tfs_close_server();
int tfs_write_server();
int tfs_read_server();
int tfs_shutdown_after_all_closed_server();

typedef struct {
    char op_code;
    int session_id, flags, fhandle, len;
    char name[40];
    char buffer[BLOCK_SIZE];
} request;

request requests_buffer[10];


int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *server_pipe_path = argv[1];
    server_pipe_name = server_pipe_path;
    printf("Starting TecnicoFS server with pipe called %s\n", server_pipe_path);

    if (unlink(server_pipe_path) != 0 && errno != ENOENT)
        exit(EXIT_FAILURE);
    
    if (mkfifo(server_pipe_path, 0777) < 0)
        exit(EXIT_FAILURE);
    
    server = open(server_pipe_path, O_RDONLY);
    if (server == -1)
        exit(EXIT_FAILURE);

    sleep(1);
    tfs_init();
    while (receive_requests() == 1)
        ;
    close(server);
    return 0;
}

int receive_requests(){
    int i = 0;
    while (true) {
        char op;
        ssize_t ret = read(server, &op, sizeof(op));
        if (ret == 0){
            close(server);
            server = open(server_pipe_name, O_RDONLY);
            return 1;
        } else if (ret != sizeof(op))
            return -1;    
        
        switch (op) {
            case (char) TFS_OP_CODE_MOUNT: i = tfs_mount_server();
                break;
            case (char) TFS_OP_CODE_UNMOUNT: i = tfs_unmount_server();
                break;
            case (char) TFS_OP_CODE_OPEN: i = tfs_open_server();
                break;
            case (char) TFS_OP_CODE_CLOSE: i = tfs_close_server();
                break;
            case (char) TFS_OP_CODE_WRITE: i = tfs_write_server();
                break;
            case (char) TFS_OP_CODE_READ: i = tfs_read_server();
                break;
            case (char) TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED: i = tfs_shutdown_after_all_closed_server();
                break;
        }
        
    }
    return i;
}


int tfs_mount_server(){
    char client_pipe_path[40];
    ssize_t ret = read(server, client_pipe_path, 40 - 1);
    for (int i = (int) ret; i < 40; i++){
        client_pipe_path[i] = '\0';
    }

    int client = open(client_pipe_path, O_WRONLY);
    if (client == -1)
        return -1;
    
    ret = write(client, &sessions, sizeof(sessions));
    if (ret != sizeof(sessions))
        return -1;
    
    open_clients[sessions++] = client;

    return 0;
}

int tfs_unmount_server(){
    int session_id;
    ssize_t ret = read(server, &session_id, sizeof(session_id));
    if (ret != sizeof(session_id))
        return -1;
        
    close(open_clients[session_id]);
    sessions--;
    return 0;
}

int tfs_open_server(){
    int session_id;
    ssize_t ret = read(server, &session_id, sizeof(session_id));
    if (ret != sizeof(session_id))
        return -1;

    char name[40];
    ret = read(server, name, sizeof(name));
    if (ret != sizeof(name))
        return -1;

    int flags;
    ret = read(server, &flags, sizeof(flags));
    if (ret != sizeof(flags))
        return -1;

    /*request request = requests_buffer[session_id];
    request.op_code = TFS_OP_CODE_OPEN;
    memcpy(request.name, name, sizeof(name));
    request.flags = flags;*/

    int fhandle = tfs_open(name, flags);

    int client = open_clients[session_id];
    ret = write(client, &fhandle, sizeof(fhandle));
    if (ret != sizeof(fhandle))
        return -1;
    
    return 0;
}

int tfs_close_server(){
    int session_id;
    ssize_t ret = read(server, &session_id, sizeof(session_id));
    if (ret != sizeof(session_id))
        return -1;
    
    int fhandle;
    ret = read(server, &fhandle, sizeof(fhandle));
    if (ret != sizeof(fhandle))
        return -1;
    
    int return_value = tfs_close(fhandle);

    int client = open_clients[session_id];
    ret = write(client, &return_value, sizeof(return_value));
    if (ret != sizeof(return_value))
        return -1;
    
    return 0;
}

int tfs_write_server(){

    int session_id;
    ssize_t ret = read(server, &session_id, sizeof(session_id));
    if (ret != sizeof(session_id))
        return -1;
    
    int fhandle;
    ret = read(server, &fhandle, sizeof(fhandle));
    if (ret != sizeof(fhandle))
        return -1;
    
    size_t len;
    ret = read(server, &len, sizeof(len));
    if (ret != sizeof(ret))
        return -1;
    

    char buffer[BLOCK_SIZE];
    size_t bytes_read = (size_t) read(server, buffer, sizeof(buffer));
	if (bytes_read == -1)
		return -1;

    int return_value = (int) tfs_write(fhandle, buffer, bytes_read);
    
    int client = open_clients[session_id];
    ret = write(client, &return_value, sizeof(return_value));
    if (ret != sizeof(return_value))
        return -1;
    
    return 0;

}
int tfs_read_server(){
    int session_id;
    ssize_t ret = read(server, &session_id, sizeof(session_id));
    if (ret != sizeof(session_id))
        return -1;
    
    int fhandle;
    ret = read(server, &fhandle, sizeof(fhandle));
    if (ret != sizeof(fhandle))
        return -1;

    size_t len;
    ret = read(server, &len, sizeof(len));
    if (ret != sizeof(len))
        return -1;

    char buffer[BLOCK_SIZE];
    int bytes_read = (int) tfs_read(fhandle, buffer, len);

    int client = open_clients[session_id];
    ret = write(client, &bytes_read, sizeof(bytes_read));
    if (ret != sizeof(bytes_read))
        return -1;
    
    ret = write(client, buffer, (size_t) bytes_read);
    if (ret != bytes_read)
        return -1;

    return 0;    
}

int tfs_shutdown_after_all_closed_server(){
    int session_id;
    ssize_t ret = read(server, &session_id, sizeof(session_id));
    if (ret != sizeof(session_id))
        return -1;

    if (tfs_destroy_after_all_closed() != 0)
        return -1;
    
    return 0;
}
/*
int escravo(request request){

    char op_code = request.op_code;
    int i;
    switch (op_code) {
        case (char) TFS_OP_CODE_MOUNT: i = tfs_mount_server();
            break;
        case (char) TFS_OP_CODE_UNMOUNT: i = tfs_unmount_server();
            break;
        case (char) TFS_OP_CODE_OPEN: i = open_escravo();
            break;
        case (char) TFS_OP_CODE_CLOSE: i = tfs_close_server();
            break;
        case (char) TFS_OP_CODE_WRITE: i = tfs_write_server();
            break;
        case (char) TFS_OP_CODE_READ: i = tfs_read_server();
            break;
        case (char) TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED: i = tfs_shutdown_after_all_closed_server();
            break;
    }

//pthread_create(..., escravo);
}

int open_escravo(request request){
    int flags = request.flags;
    tfs_open(..., flags);
}*/