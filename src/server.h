
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

/* Includes in case the compiler complains */
#include <pthread.h>
#include <semaphore.h>

/* To make sure config data type is known about. */
#include "common.h"
#include "mqtt.h"


/* Constants */
#define FULL_MESSAGE ((const char *)"KL/0.3 505 client capacity full, " \
                      "try again later\n")

enum {
    // should be at least 6
    POLL_SIZE = 11,
    LISTEN_QUEUE = 10,
    ARG_LEN = 5,
    ARG_BUF_LEN = 512,

    // in seconds
    KEEP_ALIVE = 400,

    // for when there are too many clients
    FULL_MESSAGE_LEN = 50
};

/*******************************************************************************
 * Non-specific server-related initializations will reside here.
 * such as: sharing pointers to some buffers
 ******************************************************************************/

void assign_buffers( char **srvr_buf, char **str_buf,
                     char *tpc, char *application_msg,
                     db_data *data, config *cfg, int *to_chng,
                     pthread_mutex_t *lck, sem_t *mtx );

/*******************************************************************************
 * Server function declarations will reside here.
 ******************************************************************************/
/* A way to create a socket for the kisslight server. */
int create_server_socket( const int port );

/* server's loop */
int server_loop( int listenfd );

/* a way to cleanly exit */
void close_socket();

/*******************************************************************************
 * mqtt function declarations will reside here.
 ******************************************************************************/
/* A way to create a socket for the mqtt functions. */
int open_nb_socket(const char *addr, const char *port);

int initialize_mqtt( struct mqtt_client *client, int *sockfd,
                               uint8_t *snd_buf, uint8_t *recv_buf,
                               config *conf );

void publish_kl_callback(void** client,
                         struct mqtt_response_publish *published);

void* client_refresher(void* client);

#endif
