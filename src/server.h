
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

/* To make sure config data type is known about. */
#include "common.h"
#include "mqtt.h"

/*******************************************************************************
 * Server function declarations will reside here.
 ******************************************************************************/
/* A way to create a socket for the kisslight server. */
int create_server_socket( const int port );

/* a way to cleanly exit */
void close_socket();

/*******************************************************************************
 * mqtt function declarations will reside here.
 ******************************************************************************/
/* A way to create a socket for the mqtt functions. */
int open_nb_socket(const char *addr, const char *port);
int initialize_mqtt( struct mqtt_client *client, int sockfd,
                               uint8_t *snd_buf, uint8_t *recv_buf,
                               config *conf );
void publish_kl_callback(void** unused,
                         struct mqtt_response_publish *published);

enum {
    POLL_SIZE = 12,
    LISTEN_QUEUE = 10,
    ARG_LEN = 4,
    ARG_BUF_LEN = 512,

    // in seconds
    KEEP_ALIVE = 400
};

#endif
