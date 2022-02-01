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
void* escravo();
int open_escravo();

request requests_buffer[10];
pthread_t thread[10];
int produced_req = 0, consumed_req = 0, count = 0;

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

    tfs_init();
    for (int i = 0; i < 10; i++){
        requests_buffer[i].session_id = -1;
        pthread_cond_init(&requests_buffer[i].prod, NULL);
        pthread_cond_init(&requests_buffer[i].cons, NULL);
        pthread_mutex_init(&requests_buffer[i].mutex, NULL);
        requests_buffer[i].requested = 0;
    }
    int aux_array[10];
    for (int i = 0; i < 10; i++){
        aux_array[i] = i;
    }
    for (int i = 0; i < 10; i++){
        pthread_create(&thread[i], NULL, escravo, (void*)&aux_array[i]);
    }

    while (receive_requests() == 1)
        ;

    for (int i = 0; i < 10; i++){
        pthread_join(thread[i], NULL);
    }
    close(server);
    return 0;
}

int receive_requests(){
    int i = 0;
    while (true) {
        request req;
        ssize_t ret = read(server, &req, sizeof(req));
        if (ret == 0){
            close(server);
            server = open(server_pipe_name, O_RDONLY);
            return 1;
        } else if (ret != sizeof(req)){
            return -1;
        }
        int session_id = req.session_id;

        if (session_id == -1){
            char client_pipe_path[40];
            memcpy(client_pipe_path, req.name, sizeof(req.name));
            int client = open(client_pipe_path, O_WRONLY);
            if (client == -1)
                return -1;
            ret = write(client, &sessions, sizeof(sessions));
            if (ret != sizeof(sessions))
                return -1;
            open_clients[sessions++] = client;
        } else {

            pthread_mutex_lock(&requests_buffer[session_id].mutex);
            requests_buffer[session_id].op_code = req.op_code;
            requests_buffer[session_id].flags = req.flags;
            requests_buffer[session_id].session_id = req.session_id;
            memcpy(requests_buffer[session_id].name, req.name, sizeof(req.name));
            requests_buffer[session_id].fhandle = req.fhandle;
            requests_buffer[session_id].requested = 1;
            pthread_cond_broadcast(&requests_buffer[session_id].cons);
            pthread_mutex_unlock(&requests_buffer[session_id].mutex);
            printf("ok\n");
        }

    }
    return i;
}
/*
int tfs_mount_server(){
    char client_pipe_path[40];
    ssize_t ret = read(server, client_pipe_path, 40 - 1);
    for (int i = (int) ret; i < 40; i++){
        client_pipe_path[i] = '\0';
    }

    pthread_mutex_lock(&mutex);
    while (count == 10)
        pthread_cond_wait(&produce, &mutex);
    requests_buffer[produced_req].op_code = TFS_OP_CODE_MOUNT;
    memcpy(requests_buffer[produced_req].name, client_pipe_path, sizeof(client_pipe_path));
    requests_buffer[produced_req].flags = flags;
    produced_req++;
    if (produced_req == 10)
        produced_req = 0;
    count++;
    pthread_cond_signal(&consume);
    pthread_mutex_unlock(&mutex);
    pthread_create(&thread[session_id], NULL, escravo, NULL);
    pthread_join(thread[session_id], NULL);

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

    pthread_mutex_lock(&mutex);
    while (count == 10)
        pthread_cond_wait(&produce, &mutex);
    requests_buffer[produced_req].op_code = TFS_OP_CODE_OPEN;
    memcpy(requests_buffer[produced_req].name, name, sizeof(name));
    requests_buffer[produced_req].flags = flags;
    produced_req++;
    if (produced_req == 10)
        produced_req = 0;
    count++;
    pthread_cond_signal(&consume);
    pthread_mutex_unlock(&mutex);
    pthread_create(&thread[session_id], NULL, escravo, NULL);
    pthread_join(thread[session_id], NULL);
    
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
*/

void* escravo(void* arg){
    int session_id = *((int*) arg);

    while (true){
        pthread_mutex_lock(&requests_buffer[session_id].mutex);
        while (requests_buffer[session_id].requested == 0)
            pthread_cond_wait(&requests_buffer[session_id].cons, &requests_buffer[session_id].mutex);
        printf("saiu %d\n", session_id);
        request req = requests_buffer[session_id];
        pthread_mutex_unlock(&requests_buffer[session_id].mutex);
        printf("deu %d\n", session_id);
        char op_code = req.op_code;
        printf("sessao id %d\n", req.session_id);
        int i;
        switch (op_code) {
            case (char) TFS_OP_CODE_UNMOUNT: i = tfs_unmount_server(req);
                break;
            case (char) TFS_OP_CODE_OPEN: i = tfs_open_server(requests_buffer[session_id]);
                break;
            case (char) TFS_OP_CODE_CLOSE: i = tfs_close_server(req);
                break;
            case (char) TFS_OP_CODE_WRITE: i = tfs_write_server(req);
                break;
            case (char) TFS_OP_CODE_READ: i = tfs_read_server(req);
                break;
            case (char) TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED: i = tfs_shutdown_after_all_closed_server(req);
                break;
        }
        if (i == -1)
            break;
    }
    
    return NULL;
}

int tfs_open_server(request req){

    int session_id = req.session_id;
    char name[40];
    memcpy(name, req.name, sizeof(req.name));
    int flags = req.flags;

    int fhandle = tfs_open(name, flags);

    printf("fhandle %d\n", fhandle);

    int client = open_clients[session_id];
    ssize_t ret = write(client, &fhandle, sizeof(fhandle));
    if (ret != sizeof(fhandle))
        return -1;
    
    requests_buffer[session_id].requested = 0;
    pthread_cond_broadcast(&requests_buffer[session_id].prod);
    return 0;
}


int tfs_close_server(request req){

    int session_id = req.session_id;
    int fhandle = req.fhandle;
    
    int return_value = tfs_close(fhandle);

    int client = open_clients[session_id];
    ssize_t ret = write(client, &return_value, sizeof(return_value));
    if (ret != sizeof(return_value))
        return -1;
    
    requests_buffer[session_id].requested = 0;
    pthread_cond_broadcast(&requests_buffer[session_id].prod);
    pthread_cond_broadcast(&requests_buffer[session_id].cons);

    return 0;
}

int tfs_write_server(request req){

    int session_id = req.session_id;
    int fhandle = req.fhandle;
    size_t len = req.len;
    char buffer[BLOCK_SIZE];
    memcpy(buffer, req.buffer, len);

    size_t bytes_read = strlen(buffer);
    int return_value = (int) tfs_write(fhandle, buffer, bytes_read);
    
    int client = open_clients[session_id];
    ssize_t ret = write(client, &return_value, sizeof(return_value));
    if (ret != sizeof(return_value))
        return -1;
    
    requests_buffer[session_id].requested = 0;
    pthread_cond_broadcast(&requests_buffer[session_id].prod);
    pthread_cond_broadcast(&requests_buffer[session_id].cons);

    return 0;

}

int tfs_read_server(request req){
    int session_id = req.session_id;
    int fhandle = req.fhandle;
    size_t len = req.len;

    char buffer[BLOCK_SIZE];
    memcpy(buffer, req.buffer, sizeof(req.buffer));
    int bytes_read = (int) tfs_read(fhandle, buffer, len);

    int client = open_clients[session_id];
    ssize_t ret = write(client, &bytes_read, sizeof(bytes_read));
    if (ret != sizeof(bytes_read))
        return -1;
    
    ret = write(client, buffer, (size_t) bytes_read);
    if (ret != bytes_read)
        return -1;

    requests_buffer[session_id].requested = 0;
    pthread_cond_broadcast(&requests_buffer[session_id].prod);
    pthread_cond_broadcast(&requests_buffer[session_id].cons);

    return 0;
}

int tfs_shutdown_after_all_closed_server(request req){
    tfs_destroy_after_all_closed();
    req.requested = 0;
        return -1;
}

int tfs_unmount_server(request req){
    int session_id = req.session_id;
    close(open_clients[session_id]);
    sessions--;
    requests_buffer[session_id].requested = 0;
    requests_buffer[session_id].session_id = -1;
    pthread_cond_broadcast(&requests_buffer[session_id].prod);
    pthread_cond_broadcast(&requests_buffer[session_id].cons);

    return 0;
}