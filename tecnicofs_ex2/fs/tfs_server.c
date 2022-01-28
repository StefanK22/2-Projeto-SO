#include "operations.h"

int sessions = 0;

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *server_pipe_path = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", server_pipe_path);

    if (unlink(server_pipe_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", server_pipe_path,
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    if (mkfifo(server_pipe_path, 0640) != 0)
        exit(EXIT_FAILURE);

    int server = open(server_pipe_path, O_RDONLY);
    if (server == -1)
        exit(EXIT_FAILURE);

    char client_pipe_path[40];
    size_t ret = read(server, client_pipe_path, 40 - 1);
    client_pipe_path[ret] = '\0';
    
    int client = open(client_pipe_path, O_WRONLY);
    if (client == -1)
        exit(EXIT_FAILURE);

    /*char* session_id = "0";
    ret = write(client, session_id, sizeof(session_id));
    if (ret != sizeof(session_id));
        exit(EXIT_FAILURE);
    */
   
    close(client);
    close(server);
    return 0;
}