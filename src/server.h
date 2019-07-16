
/*
 * This header is mostly used to define a default port, 
 * maximum number of connections (POLL_SIZE)
 * listenq (LISTEN_QUEUE) size, and buffer size.
 * 
 * This is also likely not the correct way to do this sort of thing,
 * but for this project it will do.
 */
#ifndef SERVER_H_
#define SERVER_H_

#define IPADDR ((const unsigned char*)"127.0.0.1")

static int create_socket();
static void connection_handler(struct pollfd *connfds, int num);
static void network_loop(int listenfd);

enum {
    PORT = 1155,
    POLL_SIZE = 32,
    LISTEN_QUEUE = 10,
    
    // in bytes
    BUFFER_SIZE = 2048,
    LOG_BUFFER_SIZE = 256
};

#endif
