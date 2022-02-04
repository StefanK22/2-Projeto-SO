#ifndef SERVER_H
#define SERVER_H

#include "operations.h"

#define MAX_SESSIONS (10)

/*
 * Reads clients' requests from the server's pipe.
 */
void receive_requests();

/*
 * Function executed by each working thread, that fulfills the request from the correspondent session ID.
 */
void* working_thread(void* arg);

/*
 * Unmounts a client from the server.
 */
void tfs_unmount_server(request req);

/*
 * Opens a file requested by the client.
 */
void tfs_open_server(request req);

/*
 * Closes a file requested by the client.
 */
void tfs_close_server(request req);

/*
 * Writes to an open file requested by the client.
 */
void tfs_write_server(request req);

/*
 * Reads from an open file requested by the client.
 */
void tfs_read_server(request req);

/*
 * Waits until no file is open and then shuts down the server.
 */
void tfs_shutdown_after_all_closed_server(request req);

#endif // SERVER_H
