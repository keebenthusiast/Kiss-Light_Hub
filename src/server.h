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


/* Constants for server */
#define KL_VERSION 0.3

// response strings (in order)
#define MESSAGE_200 ((const char *)"KL/%.1f 200 device %s power toggled\n")
#define MESSAGE_201 ((const char *)"KL/%.1f 201 device %s %s %s set\n")
#define MESSAGE_202 ((const char *)"KL/%.1f 202 device %s added\n")
#define MESSAGE_203 ((const char *)"KL/%.1f 203 device %s deleted\n")
#define MESSAGE_204 ((const char *)"KL/%.1f 204 number of devices: %d\n")
#define DUMP_204    ((const char *)"%s -- %s -- %s\n")
#define MESSAGE_205 ((const char *)"KL/%.1f 205 custom command %s %s sent\n")
#define MESSAGE_206 ((const char *)"KL/%.1f 206 device %s state:\n")
#define MESSAGE_207 ((const char *)"KL/%.1f 207 goodbye\n")
#define MESSAGE_208 ((const char *)"KL/%.1f 208 dev_name %s updated to %s\n")
#define MESSAGE_209 ((const char *)"KL/%.1f 209 dev_name %s mqtt_topic " \
"updated to %s\n")
#define MESSAGE_210 ((const char *)"KL/%.1f 210 dev_name %s dev_state " \
"updated\n")

#define MESSAGE_400 ((const char *)"KL/%.1f 400 bad request\n")
//#define MESSAGE_401 ((const char *)"KL/%.1f 401 device %s state unknown\n")
#define MESSAGE_402 ((const char *)"KL/%.1f 402 unable to delete %s\n")
#define MESSAGE_403 ((const char *)"KL/%.1f 403 unable to add device %s\n")
#define MESSAGE_404 ((const char *)"KL/%.1f 404 no such device %s\n")
#define MESSAGE_405 ((const char *)"KL/%.1f 405 incorrect input %s\n")
#define MESSAGE_406 ((const char *)"KL/%.1f 406 cannot detect KL version\n")
#define MESSAGE_407 ((const char *)"KL/%.1f 407 not yet implemented\n")
#define MESSAGE_408 ((const char *)"KL/%.1f 408 device %s already exists\n")
#define MESSAGE_409 ((const char *)"KL/%.1f 409 not enough args passed in\n")

#define MESSAGE_500 ((const char *)"KL/%.1f 500 internal error: %s\n")
#define MESSAGE_505 ((const char *)"KL/0.3 505 client capacity full, " \
                    "try again later\n")


/* relating to requests */
#define TRANSMIT    ((const char *)"TRANSMIT")
#define TOGGLE      ((const char *)"TOGGLE")
#define SET_REQ     ((const char *)"SET")
#define ADD_REQ     ((const char *)"ADD")
#define DEL_REQ     ((const char *)"DELETE")
#define UPDATE_REQ  ((const char *)"UPDATE")
#define UPDATE_A    ((const char *)"NAME")
#define UPDATE_B    ((const char *)"TOPIC")
#define UPDATE_C    ((const char *)"STATE")
#define LIST        ((const char *)"LIST")
#define STATUS      ((const char *)"STATUS")
#define QA          ((const char *)"Q")
#define QB          ((const char *)"QUIT")

/* Constants for MQTT */
// mqtt topic prefix
#define STAT        ((const char *)"stat/")
#define CMND        ((const char *)"cmnd/")

// mqtt topic suffix
#define RESULT      ((const char *)"/RESULT")
#define STATE       ((const char *)"/STATE")
#define MQTT_UPDATE ((const char *)"/TOPIC")
#define POWER       ((const char *)"/POWER")

enum {
    // should be at least 2
    POLL_SIZE = 11,
    LISTEN_QUEUE = 10,
    ARG_BUF_LEN = 256,
    ARG_LEN = 6,

    // in seconds
    KEEP_ALIVE = 400,

    /*
     * Response messages
     */
    MESSAGE_200_LEN = 34,
    MESSAGE_201_LEN = 26,
    MESSAGE_202_LEN = 26,
    MESSAGE_203_LEN = 28,
    MESSAGE_204_LEN = 32,
    DUMP_204_LEN = 10,
    MESSAGE_205_LEN = 34,
    MESSAGE_206_LEN = 27,
    MESSAGE_207_LEN = 20,
    MESSAGE_208_LEN = 34,
    MESSAGE_209_LEN = 45,
    MESSAGE_210_LEN = 40,
    MESSAGE_400_LEN = 24,
    //MESSAGE_401_LEN = 34,
    MESSAGE_402_LEN = 30,
    MESSAGE_403_LEN = 34,
    MESSAGE_404_LEN = 28,
    MESSAGE_405_LEN = 29,
    MESSAGE_406_LEN = 37,
    MESSAGE_407_LEN = 32,
    MESSAGE_408_LEN = 35,
    MESSAGE_409_LEN = 38,
    MESSAGE_500_LEN = 29,
    MESSAGE_505_LEN = 50,

    // for requests
    TRANSMIT_LEN = 8,
    TOGGLE_LEN = 6,
    SET_REQ_LEN = 3,
    ADD_REQ_LEN = 3,
    DEL_REQ_LEN = 6,
    UPDATE_REQ_LEN = 6,
    UPDATE_A_LEN = 5,
    UPDATE_B_LEN = 6,
    UPDATE_C_LEN = 6,
    LIST_LEN = 4,
    STATUS_LEN = 6,
    QA_LEN = 1,
    QB_LEN = 4,

    // expected arg counts for each request type
    TRANSMIT_ARG = 4,
    TOGGLE_ARG = 3,
    SET_ARG = 5,
    ADD_ARGA = 5,
    ADD_ARGB = 6,
    DELETE_ARG = 3,
    UPDATE_ARG = 4,
    LIST_ARG = 2,
    STATUS_ARG = 3,

    // prefix (for topics)
    STAT_LEN = 6,
    CMND_LEN = 6,

    // result suffix (for topics)
    RESULT_LEN = 8,
    STATE_LEN = 7,
    MQTT_UPDATE_LEN = 7,
    POWER_LEN = 7

};

/*******************************************************************************
 * Non-specific server-related initializations will reside here.
 * such as: sharing pointers to some buffers
 *
 * Also where misc functions will reside as well.
 ******************************************************************************/

void assign_buffers( char **srvr_buf, char *tpc, char *application_msg,
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
