/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019-2021, Christian Kissinger
 * kiss-light Hub is released under the New BSD license (see LICENSE).
 * Go to the project repo here:
 * https://gitlab.com/kiss-light-project/Kiss-Light_Hub
 *
 * Written by: Christian Kissinger
 */

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
#include <poll.h>

/* To make sure config data type is known about. */
#include "config.h"
#include "database.h"
#include "mqtt.h"


/* Useful Constants */
#define FULL_MESSAGE ((const char *)"KL/0.3 505 client capacity full, " \
                      "try again later\n")
#define KL_VERSION 0.3

// mqtt topic prefix
#define STAT         ((const char *)"stat/")
#define CMND         ((const char *)"cmnd/")

// mqtt topic suffix
#define RESULT       ((const char *)"/RESULT")

enum {
    // should be at least 2
    POLL_SIZE = 11,
    LISTEN_QUEUE = 10,
    ARG_LEN = 5,
    ARG_BUF_LEN = 512,

    // in seconds
    KEEP_ALIVE = 400,

    // for when there are too many clients
    FULL_MESSAGE_LEN = 50,

    // prefix (for topics)
    STAT_LEN = 6,
    CMND_LEN = 6,

    // result suffix (for topics)
    RESULT_LEN = 8

};

/*******************************************************************************
 * Non-specific server-related initializations will reside here.
 * such as: sharing pointers to some buffers
 *
 * Also where misc functions will reside as well.
 ******************************************************************************/

void assign_buffers( char **srvr_buf, char **str_buf,
                     char *tpc, char *application_msg,
                     db_data *data, config *cfg, int *to_chng,
                     pthread_mutex_t *lck, sem_t *mtx,
                     struct pollfd *czfds );

void prepare_topic( const char *prefix, const char *tpc,
                    char *suffix );

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
