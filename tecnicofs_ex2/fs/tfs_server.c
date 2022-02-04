#include "operations.h"

int sessions = 0;
int open_clients[MAX_SESSIONS];
int server;
char* server_pipe_name;


void receive_requests();
int tfs_mount_server();
int tfs_unmount_server();
int tfs_open_server();
int tfs_close_server();
int tfs_write_server();
int tfs_read_server();
int tfs_shutdown_after_all_closed_server();
void* working_thread();

request requests_buffer[MAX_SESSIONS];
pthread_t thread[MAX_SESSIONS];

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

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

    if (tfs_init() != 0)
        exit(EXIT_FAILURE);
        
    for (int i = 0; i < MAX_SESSIONS; i++){
        open_clients[i] = -1;
    }
    
    for (int i = 0; i < MAX_SESSIONS; i++){
        if (pthread_cond_init(&requests_buffer[i].prod, NULL) != 0)
            exit(EXIT_FAILURE);
    
        if (pthread_cond_init(&requests_buffer[i].cons, NULL) != 0)
            exit(EXIT_FAILURE);
        
        if (pthread_mutex_init(&requests_buffer[i].mutex, NULL) != 0)
            exit(EXIT_FAILURE);
        
        requests_buffer[i].session_id = -1;
        requests_buffer[i].requested = 0;
    }
    int aux_array[MAX_SESSIONS];
    for (int i = 0; i < MAX_SESSIONS; i++){
        aux_array[i] = i;
    }
    for (int i = 0; i < MAX_SESSIONS; i++){
        if (pthread_create(&thread[i], NULL, working_thread, (void*)&aux_array[i]) != 0)
            exit(EXIT_FAILURE);
    }

    receive_requests();

    for (int i = 0; i < MAX_SESSIONS; i++){
        if (pthread_mutex_lock(&requests_buffer[i].mutex) != 0)
            exit(EXIT_FAILURE);

        while (requests_buffer[i].requested == 1)
            if (pthread_cond_wait(&requests_buffer[i].prod, &requests_buffer[i].mutex) != 0)
            exit(EXIT_FAILURE);

        requests_buffer[i].op_code = -1;
        requests_buffer[i].requested = 1;
        if (pthread_cond_broadcast(&requests_buffer[i].cons) != 0)
            exit(EXIT_FAILURE);

        if (pthread_mutex_unlock(&requests_buffer[i].mutex) != 0)
            exit(EXIT_FAILURE);

    }

    for (int i = 0; i < MAX_SESSIONS; i++){
        if (pthread_join(thread[i], NULL) != 0)
            exit(EXIT_FAILURE);
    }
    if (close(server) != 0)
        exit(EXIT_FAILURE);
        
    return 0;
}

void receive_requests(){
    while (true) {
        request req;
        ssize_t ret = read(server, &req, sizeof(req));
        if (ret == 0){
            close(server);
            server = open(server_pipe_name, O_RDONLY);
            if (server == -1)
                exit(EXIT_FAILURE);
            continue;
        } else if (ret != sizeof(req))
            return;
        int session_id = req.session_id;

        if (session_id == -1){
            //Mount
            char client_pipe_path[NAME_SIZE];
            memcpy(client_pipe_path, req.name, sizeof(req.name));
            int client = open(client_pipe_path, O_WRONLY);
            if (client == -1)
                return;
            if (sessions < MAX_SESSIONS){
                ret = write(client, &sessions, sizeof(sessions));
                if (ret != sizeof(sessions))
                    return;
                for (int i = 0; i < MAX_SESSIONS; i++)
                    if (open_clients[i] == -1){
                        open_clients[i] = client;
                        sessions++;
                        break;
                    }
            }
            else {
                int return_value = -1;
                ret = write(client, &return_value, sizeof(return_value));
                if (ret != sizeof(return_value))
                    return;
            }
        } else {

            if (pthread_mutex_lock(&requests_buffer[session_id].mutex) != 0)
                exit(EXIT_FAILURE);
            while (requests_buffer[session_id].requested == 1)
                if (pthread_cond_wait(&requests_buffer[session_id].prod, &requests_buffer[session_id].mutex) != 0)
                    exit(EXIT_FAILURE);

            requests_buffer[session_id].op_code = req.op_code;
            requests_buffer[session_id].flags = req.flags;
            requests_buffer[session_id].session_id = req.session_id;
            requests_buffer[session_id].fhandle = req.fhandle;
            requests_buffer[session_id].len = req.len;
            memcpy(requests_buffer[session_id].name, req.name, sizeof(req.name));
            memcpy(requests_buffer[session_id].buffer, req.buffer, sizeof(req.buffer));
            requests_buffer[session_id].requested = 1;

            if (pthread_cond_broadcast(&requests_buffer[session_id].cons) != 0)
                exit(EXIT_FAILURE);
            if (pthread_mutex_unlock(&requests_buffer[session_id].mutex) != 0)
                exit(EXIT_FAILURE);

            if (req.op_code == (char) TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED){
                if (pthread_mutex_lock(&requests_buffer[session_id].mutex) != 0)
                    exit(EXIT_FAILURE);
                while (requests_buffer[session_id].requested == 1)
                    if (pthread_cond_wait(&requests_buffer[session_id].prod, &requests_buffer[session_id].mutex) != 0)
                        exit(EXIT_FAILURE);
                if (pthread_mutex_unlock(&requests_buffer[session_id].mutex) != 0)
                    exit(EXIT_FAILURE);
                return;
            }
        }

    }
}


void* working_thread(void* arg){
    int session_id = *((int*) arg);

    while (true){
        if (pthread_mutex_lock(&requests_buffer[session_id].mutex) != 0)
            exit(EXIT_FAILURE);
        while (requests_buffer[session_id].requested == 0)
            if (pthread_cond_wait(&requests_buffer[session_id].cons, &requests_buffer[session_id].mutex) != 0)
                exit(EXIT_FAILURE);
        request req = requests_buffer[session_id];
        char op_code = req.op_code;
        int i = 0;
        switch (op_code) {
            case (char) TFS_OP_CODE_UNMOUNT: i = tfs_unmount_server(req);
                break;
            case (char) TFS_OP_CODE_OPEN: i = tfs_open_server(req);
                break;
            case (char) TFS_OP_CODE_CLOSE: i = tfs_close_server(req);
                break;
            case (char) TFS_OP_CODE_WRITE: i = tfs_write_server(req);
                break;
            case (char) TFS_OP_CODE_READ: i = tfs_read_server(req);
                break;
            case (char) TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED: i = tfs_shutdown_after_all_closed_server(req);
                break;
            case (char) -1: {
                    if (pthread_mutex_unlock(&requests_buffer[session_id].mutex) != 0)
                        exit(EXIT_FAILURE);
                    return NULL;
                }
            default: return NULL;
        }
        if (i == -1){
            if (pthread_mutex_unlock(&requests_buffer[session_id].mutex) != 0)
                exit(EXIT_FAILURE);
            break;
        }
    }
    
    return NULL;
}

int tfs_open_server(request req){

    int session_id = req.session_id;
    char name[NAME_SIZE];
    memcpy(name, req.name, sizeof(req.name));
    int flags = req.flags;

    int fhandle = tfs_open(name, flags);

    int client = open_clients[session_id];

    ssize_t ret = write(client, &fhandle, sizeof(fhandle));
    if (ret != sizeof(fhandle))
        tfs_unmount_server(req);
    
    requests_buffer[session_id].requested = 0;
    if (pthread_cond_broadcast(&requests_buffer[session_id].prod) != 0)
        exit(EXIT_FAILURE);
    if (pthread_mutex_unlock(&requests_buffer[session_id].mutex) != 0)
        exit(EXIT_FAILURE);
    return 0;
}


int tfs_close_server(request req){

    int session_id = req.session_id;
    int fhandle = req.fhandle;
    
    int return_value = tfs_close(fhandle);

    int client = open_clients[session_id];
    ssize_t ret = write(client, &return_value, sizeof(return_value));
    if (ret != sizeof(return_value))
        tfs_unmount_server(req);
    
    requests_buffer[session_id].requested = 0;
    if (pthread_cond_broadcast(&requests_buffer[session_id].prod) != 0)
        exit(EXIT_FAILURE);
    if (pthread_mutex_unlock(&requests_buffer[session_id].mutex) != 0)
        exit(EXIT_FAILURE);

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
        tfs_unmount_server(req);
    
    requests_buffer[session_id].requested = 0;
    if (pthread_cond_broadcast(&requests_buffer[session_id].prod) != 0)
        exit(EXIT_FAILURE);
    if (pthread_mutex_unlock(&requests_buffer[session_id].mutex) != 0)
        exit(EXIT_FAILURE);
    return 0;

}

int tfs_read_server(request req){
    int session_id = req.session_id;
    int fhandle = req.fhandle;
    size_t len = req.len;

    char buffer[BLOCK_SIZE];
    int bytes_read = (int) tfs_read(fhandle, buffer, len);

    int client = open_clients[session_id];
    ssize_t ret = write(client, &bytes_read, sizeof(bytes_read));
    if (ret != sizeof(bytes_read))
        tfs_unmount_server(req);
    
    ret = write(client, buffer, (size_t) bytes_read);
    if (ret != bytes_read)
        tfs_unmount_server(req);

    requests_buffer[session_id].requested = 0;
    if (pthread_cond_broadcast(&requests_buffer[session_id].prod) != 0)
        exit(EXIT_FAILURE);
    if (pthread_mutex_unlock(&requests_buffer[session_id].mutex) != 0)
        exit(EXIT_FAILURE);

    return 0;
}

int tfs_shutdown_after_all_closed_server(request req){
    int session_id = req.session_id;

    int return_value = tfs_destroy_after_all_closed();

    int client = open_clients[session_id];
    ssize_t ret = write(client, &return_value, sizeof(return_value));
    if (ret != sizeof(return_value))
        tfs_unmount_server(req);
    
    requests_buffer[session_id].requested = 0;
    if (pthread_cond_broadcast(&requests_buffer[session_id].prod) != 0)
        exit(EXIT_FAILURE);
    if (pthread_mutex_unlock(&requests_buffer[session_id].mutex) != 0)
        exit(EXIT_FAILURE);

    return 0;
}

int tfs_unmount_server(request req){
    int session_id = req.session_id;
    close(open_clients[session_id]);
    sessions--;
    open_clients[session_id] = -1;
    requests_buffer[session_id].session_id = -1;
    requests_buffer[session_id].requested = 0;
    if (pthread_cond_broadcast(&requests_buffer[session_id].prod) != 0)
        exit(EXIT_FAILURE);
    if (pthread_mutex_unlock(&requests_buffer[session_id].mutex) != 0)
        exit(EXIT_FAILURE);

    return 0;
}