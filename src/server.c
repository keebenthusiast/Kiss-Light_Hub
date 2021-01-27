/*
 * The server which has the job of handling mqtt publishes and subscriptions,
 * and whatever a user may request a connected mqtt device to handle.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019-2021, Christian Kissinger
 * kiss-light Hub is released under the New BSD license (see LICENSE).
 * Go to the project repo here:
 * https://gitlab.com/kiss-light-project/Kiss-Light_Hub
 *
 * Written by: Christian Kissinger
 */

// system-related includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>

// socket-related includes
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/wait.h>
#include <fcntl.h>

// local includes
#include "server.h"
#include "log.h"
#include "common.h"
#include "daemon.h"
#include "mqtt.h"

// pointer to config cfg;
static config *conf;

// pointers for database functions.
static db_data *memory;
static int *to_change;

// pointers for various server buffers
static char **server_buffer;
static char **str_buffer;

// mqtt buffers
static char *topic;
static char *app_msg;

// mqtt client pointer
 struct mqtt_client *cl;

// System pointers
static pthread_mutex_t *lock;
static sem_t *mutex;

/*
 * When exiting, close server's socket,
 * using this variable
 */
static unsigned int closeSocket = 0;

/* local prototypes as needed */
static int get_dev_state( const char *dv_name );
static int get_dev_state_mqtt( const char *dv_name );
static int change_dev_state( const char *dv_name, const char *msg );
static int add_device( char *dv_name, char *mqtt_tpc, const int dv_type );
static int delete_device( char *dv_name );

/*******************************************************************************
 * Non-specific server-related initializations will reside here.
 * such as: sharing pointers to some buffers
 ******************************************************************************/

/**
 * @brief assign buffers and mutex semaphores to the server.
 *
 * @param srvr_buf The server's buffer.
 * @param str_buf The parser's buffer.
 * @param tpc the topic buffer.
 * @param application_msg the application message buffer.
 * @param data the struct array of database entries.
 * @param cfg the configuration struct
 * @param to_chng the int array to indicate what is to be updated
 * @param lck the pthread mutex lock
 * @param mtx the semaphore mutex lock
 *
 */
void assign_buffers( char **srvr_buf, char **str_buf,
                     char *tpc, char *application_msg,
                     db_data *data, config *cfg, int *to_chng,
                     pthread_mutex_t *lck, sem_t *mtx )
{
    server_buffer = srvr_buf;
    str_buffer = str_buf;
    topic = tpc;
    app_msg = application_msg;
    memory = data;
    to_change = to_chng;
    conf = cfg;
    lock = lck;
    mutex = mtx;
}

/*******************************************************************************
 * Everything related to message parsing will reside here.
 ******************************************************************************/

/**
 * @brief clean str_buffer for later use.
 */
static void clean_str_buffers()
{
    for ( int i = 0; i < ARG_LEN; i++ )
    {
        memset( str_buffer[i], 0, ARG_BUF_LEN );
    }
}

/**
 * @brief Check protocol version.
 *
 * @param buf the buffer that contains the protocol version.
 *
 * @note example being: KL/0.3 .
 * Return protocol_version if valid
 * or -1.0 if invalid.
 */
static float get_protocol_version(char *buf)
{
    float protocol;

    /*
     * check to make sure buf isn't empty and first part equals KL or
     * something along those lines
     */
    if ( strcmp(buf, "") == 0 || strncasecmp( buf, "KL", 2) != 0 )
    {
        return -1.0;
    }

    /* Now extract the protocol version itself */
    if ( strncmp( buf, "kl", 2) == 0 )
    {
        sscanf( buf, "kl/%f", &protocol );
    }
    else if ( strncmp( buf, "Kl", 2) == 0 )
    {
        sscanf( buf, "Kl/%f", &protocol );
    }
    else if ( strncmp( buf, "kL", 2 ) == 0 )
    {
        sscanf( buf, "kL/%f", &protocol );
    }
    else
    {
        sscanf( buf, "KL/%f", &protocol );
    }

    /* make sure the protocol version is correct */
    if ( protocol <= 0.0 )
    {
        return -1.0;
    }

    return protocol;
}

/**
 * @brief Parse whatever the client sent in,
 * Do the task if applicable,
 * And send over the proper response.
 *
 * @param buf the buffer to be analyzed, THEN MODIFIED.
 * @param n the buffer length, modified when buf is modified.
 *
 * @note returns -1 to exit,
 * returns 1 to dump current setups
 * returns 0 for all is good.
 */
static int parse_server_request(char *buf, int *n)
{
    int rv = 0; /* the return value */

    /* process up to 5 arguments passed in */
    sscanf( buf, "%s %s %s %s %s", str_buffer[0], str_buffer[1],
            str_buffer[2], str_buffer[3], str_buffer[4] );

    /* clean buffer for reuse */
    memset( buf, 0, conf->buffer_size );

    /* Use below for features not yet implemented! */
    //*n = snprintf( buf, 32, "KL/%.1f 407 Not Yet Implemented\n",
    //               KL_VERSION );

    // TRANSMIT custom_topic custom_message KL/version#
    if ( strncasecmp(str_buffer[0], "TRANSMIT", 9) == 0 )
    {
        if( get_protocol_version(str_buffer[3]) < 0.1 )
        {
            *n = snprintf( buf, 37, "KL/%.1f 406 Cannot Detect KL version\n",
                           KL_VERSION );

            clean_str_buffers();

            return rv;
        }

        mqtt_publish( cl, str_buffer[1], str_buffer[2],
                      strlen(str_buffer[2]), MQTT_PUBLISH_QOS_0 );

        if ( cl->error != MQTT_OK )
        {
            log_warn( "mqtt error: %s", mqtt_error_str(cl->error) );
            *n = snprintf( buf, (29 + strlen(mqtt_error_str(cl->error))),
                           "KL/%.1f 500 Internal Error: %s\n", KL_VERSION,
                           mqtt_error_str(cl->error) );
        }
        else
        {
            *n = snprintf( buf, (36 + strlen(str_buffer[1]) +
                           strlen(str_buffer[2])),
                           "KL/%.1f 200 Custom Command '%s %s' Sent\n",
                           KL_VERSION, str_buffer[1], str_buffer[2] );
        }
    }
    // ADD dev_name mqtt_topic dev_type KL/version#
    else if ( strncasecmp(str_buffer[0], "ADD", 4) == 0 )
    {
        if( get_protocol_version(str_buffer[4]) < 0.1 )
        {
            *n = snprintf( buf, 37, "KL/%.1f 406 Cannot Detect KL version\n",
                           KL_VERSION );

            clean_str_buffers();

            return rv;
        }

        int status = add_device( str_buffer[1], str_buffer[2],
                                 atoi(str_buffer[3]) );

        if ( status )
        {
            *n = snprintf( buf, (36 + strlen(str_buffer[1])),
                           "KL/%.1f 403 unable to add device '%s'\n",
                           KL_VERSION, str_buffer[1] );
        }
        else
        {
            *n = snprintf( buf, (28 + strlen(str_buffer[1])),
                          "KL/%.1f 200 Device '%s' added\n",
                          KL_VERSION, str_buffer[1] );
        }
    }
    // TOGGLE dev_name KL/version#
    else if ( strncasecmp(str_buffer[0], "TOGGLE", 7) == 0 )
    {
        if( get_protocol_version(str_buffer[2]) < 0.1 )
        {
            *n = snprintf( buf, 37, "KL/%.1f 406 Cannot Detect KL version\n",
                           KL_VERSION );

            clean_str_buffers();

            return rv;
        }

        int status = change_dev_state( str_buffer[1], "toggle" );

        if ( status )
        {
            *n = snprintf( buf, (30 + strlen(str_buffer[1])),
                           "KL/%.1f 404 no such device '%s'\n",
                           KL_VERSION, str_buffer[1] );
        }
        else
        {
            *n = snprintf( buf, (28 + strlen(str_buffer[1])),
                           "KL/%.1f 200 device %s toggled\n",
                           KL_VERSION, str_buffer[1] );
        }
    }
    // SET dev_name [ON,1,TRUE|OFF,0,FALSE] KL/version#
    else if ( strncasecmp(str_buffer[0], "SET", 4) == 0 )
    {
        if( get_protocol_version(str_buffer[3]) < 0.1 )
        {
            *n = snprintf( buf, 37, "KL/%.1f 406 cannot detect KL version\n",
                           KL_VERSION );

            clean_str_buffers();

            return rv;
        }

        int status = change_dev_state( str_buffer[1], str_buffer[2] );

        if ( status == 1 )
        {
            *n = snprintf( buf, (30 + strlen(str_buffer[1])),
                           "KL/%.1f 404 no such device '%s'\n",
                           KL_VERSION, str_buffer[1] );
        }
        else if ( status == 2 )
        {
            *n = snprintf( buf, (31 + strlen(str_buffer[2])),
                           "KL/%.1f 405 incorrect input '%s'\n",
                           KL_VERSION, str_buffer[2] );
        }
        else
        {
            *n = snprintf( buf, (25 + strlen(str_buffer[1]) +
                           strlen(str_buffer[2])),
                           "KL/%.1f 200 device %s set %s\n",
                           KL_VERSION, str_buffer[1], str_buffer[2] );
        }
    }
    // LIST KL/version#
    else if ( strncasecmp(str_buffer[0], "LIST", 5) == 0 )
    {
        if( get_protocol_version(str_buffer[1]) < 0.1 )
        {
            *n = snprintf( buf, 37, "KL/%.1f 406 Cannot Detect KL version\n",
                           KL_VERSION );

            clean_str_buffers();

            return rv;
        }

        *n = snprintf( buf, (32 + get_digit_count(get_current_entry_count())),
                       "KL/%.1f 200 number of Devices: %d\n",
                       KL_VERSION, get_current_entry_count() );

        /* show the current devices to the client */
        rv = 1;
    }
    // DELETE dev_name KL/version#
    else if ( strncasecmp(str_buffer[0], "DELETE", 7) == 0 )
    {
        if( get_protocol_version(str_buffer[2]) < 0.1 )
        {
            *n = snprintf( buf, 37, "KL/%.1f 406 Cannot Detect KL version\n",
                           KL_VERSION );

            clean_str_buffers();

            return rv;
        }

        int status = delete_device( str_buffer[1] );

        if ( status )
        {
            *n = snprintf( buf, (32 + strlen(str_buffer[1])),
                           "KL/%.1f 402 unable to delete '%s'\n",
                           KL_VERSION, str_buffer[1] );
        }
        else
        {
            *n = snprintf( buf, (30 + strlen(str_buffer[1])),
                          "KL/%.1f 200 Device '%s' deleted\n",
                          KL_VERSION, str_buffer[1] );
        }
    }
    // STATUS dev_name KL/version#
    else if ( strncasecmp(str_buffer[0], "STATUS", 7) == 0 )
    {
        if( get_protocol_version(str_buffer[2]) < 0.1 )
        {
            *n = snprintf( buf, 37, "KL/%.1f 406 Cannot Detect KL version\n",
                           KL_VERSION );

            clean_str_buffers();

            return rv;
        }


        int state = get_dev_state( str_buffer[1] );

        /* check for status */
        if ( state < -1 )
        {
            *n = snprintf( buf, (30 + strlen(str_buffer[1])),
                           "KL/%.1f 404 no such device '%s'\n",
                           KL_VERSION, str_buffer[1] );
        }
        else if ( state == -1 )
        {
            /* try again, but using mqtt */
            state = get_dev_state_mqtt( str_buffer[1] );

            /* check for status */
            if ( state == -1 )
            {
                *n = snprintf( buf, (36 + strlen(str_buffer[1])),
                               "KL/%.1f 401 device '%s' state unknown\n",
                               KL_VERSION, str_buffer[1] );
            }
            else
            {
                int result_len = (state) ? 3 : 4;
                *n = snprintf( buf, (26 + strlen(str_buffer[1]) + result_len),
                               "KL/%.1f 200 device '%s' is %s\n", KL_VERSION,
                               str_buffer[1], (state) ? "ON" : "OFF" );
            }
        }
        else
        {
            int result_len = (state) ? 3 : 4;
            *n = snprintf( buf, (26 + strlen(str_buffer[1]) + result_len),
                           "KL/%.1f 200 device '%s' is %s\n", KL_VERSION,
                           str_buffer[1], (state) ? "ON" : "OFF" );
        }
    }
    // allow the client to disconnect
    else if ( strncasecmp(str_buffer[0], "Q", 2) == 0
           || strncasecmp(str_buffer[0], "QUIT", 5) == 0 )
    {
        *n = snprintf( buf, 20, "KL/%.1f 200 Goodbye\n", KL_VERSION );

        /* tell the connection handler to let the client exit */
        rv = -1;
    }
    // Something else was passed in
    else
    {
        *n = snprintf( buf, 24, "KL/%.1f 400 Bad Request\n", KL_VERSION );
    }

    /* Clean str buffers */
    clean_str_buffers();

    return rv;
}

/*******************************************************************************
 * Everything related to the server will reside here.
 ******************************************************************************/

/**
 * @brief get current device's state.
 *
 * @param dv_name the device name of interest.
 *
 * @note Returns -2 for no such device error, otherwise returns
 * the state.
 *
 * @todo this may have to be specified for toggleable devices,
 * with another one entirely for RGB bulbs.
 */
static int get_dev_state( const char *dv_name )
{
    int rv = -2; /* return value */

    sem_wait( mutex );
    pthread_mutex_lock( lock );

    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        if ( strncasecmp(memory[i].dev_name, dv_name, strlen(dv_name)) == 0 )
        {
            rv = memory[i].dev_state;
            break;
        }
    }

    pthread_mutex_unlock( lock );
    sem_post( mutex );

    return rv;
}

/**
 * @brief get current device's state, but asking the device itself.
 *
 * @param dv_name the device name of interest.
 *
 * @note Returns -2 for no such device error, otherwise returns
 * the state.
 *
 * @todo this may have to be specified for toggleable devices,
 * with another one entirely for RGB bulbs.
 */
static int get_dev_state_mqtt( const char *dv_name )
{
    int rv = -2; /* return value */

    sem_wait( mutex );
    pthread_mutex_lock( lock );

    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        if ( strncasecmp(memory[i].dev_name, dv_name, strlen(dv_name)) == 0 )
        {
            mqtt_publish( cl, memory[i].cmnd_topic, "", 0,
                          MQTT_PUBLISH_QOS_0 );
            break;
        }
    }

    pthread_mutex_unlock( lock );
    sem_post( mutex );

    /* sleep for 150ms */
    usleep( 150000U );

    return rv;
}

/**
 * @brief Change device's state.
 *
 * @param dv_name the device name to change
 * @param msg the message to change dev state
 *
 * @note Returns 1 for no such device, Returns 2 for invalid mqtt
 * command, otherwise returns 0.
 */
static int change_dev_state( const char *dv_name, const char *msg )
{
    int rv = 0; /* return value */
    int loc = -1; /* location of a device match */

    /*
     * This may have to be expanded for rgb bulbs, but
     * we shall see.
     */
    /* make sure msg is something acceptable */
    if ( strncasecmp(msg, "ON", 3) == 0
      || strncasecmp(msg, "1", 2) == 0
      || strncasecmp(msg, "TRUE", 5) == 0
      || strncasecmp(msg, "OFF", 4) == 0
      || strncasecmp(msg, "0", 2) == 0
      || strncasecmp(msg, "FALSE", 6) == 0
      || strncasecmp(msg, "TOGGLE", 7) == 0 )
    {
        rv = 0;
    }
    else
    {
        return 2;
    }

    /* Only when this point is memory going to be accessed */
    sem_wait( mutex );
    pthread_mutex_lock( lock );

    /* scan for a dev_name match */
    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        if ( strncasecmp(memory[i].dev_name, dv_name, strlen(dv_name)) == 0 )
        {
            loc = i;
            break;
        }

        /* No matches at all? all done */
        if ( i == (conf->max_dev_count - 1) )
        {
            rv = 1;
        }
    }

    /* if a device is found, send the command! */
    if ( !rv )
    {
        mqtt_publish( cl, memory[loc].cmnd_topic, msg, strlen(msg),
                      MQTT_PUBLISH_QOS_0 );
    }

    pthread_mutex_unlock( lock );
    sem_post( mutex );

    return rv;
}

/**
 * @brief Function that adds a device to memory,
 * then eventually the database.
 *
 * @param dv_name the device name of the new device
 * @param mqtt_tpc the mqtt_topic associated with the new device
 * @param dv_type the type of device the new device is
 *
 * @note Returns 1 for failure to add device
 * (usually because too many devices), returns 0 otherwise.
 */
static int add_device( char *dv_name, char *mqtt_tpc, const int dv_type )
{
    int rv = 1; /* return value */

    sem_wait( mutex );
    pthread_mutex_lock( lock );

    /* Scan for an empty spot */
    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        /* Found a spot yay */
        if ( memory[i].dev_name[0] == '\0' )
        {
            /* Copy necessary information */
            strncpy( memory[i].dev_name, dv_name, strlen(dv_name) );
            strncpy( memory[i].mqtt_topic, mqtt_tpc, strlen(mqtt_tpc) );
            memory[i].dev_type = dv_type;

            /* Also copy full topic as well */
            snprintf( memory[i].stat_topic, (12 + strlen(mqtt_tpc)),
                      "stat/%s/POWER", mqtt_tpc );
            snprintf( memory[i].cmnd_topic, (12 + strlen(mqtt_tpc)),
                      "cmnd/%s/POWER", mqtt_tpc );

            /* subscribe to this new device */
            mqtt_subscribe( cl, memory[i].stat_topic, 0 );

            /* add this device to database! */
            to_change[i] = 5;

            /* set return value to success */
            rv = 0;

            /* all done */
            break;
        }
    }

    pthread_mutex_unlock( lock );
    sem_post( mutex );

    return rv;
}

/**
 * @brief Function that adds a device to memory,
 * then eventually the database.
 *
 * @param dv_name the device name of the device to remove
 *
 * @note Returns 1 for failure to add device
 * (usually because too many devices), returns 0 otherwise.
 */
static int delete_device( char *dv_name )
{
    int rv = 1; /* return value */

    sem_wait( mutex );
    pthread_mutex_lock( lock );


    /* scan for a dev_name match */
    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        if ( strncasecmp(memory[i].dev_name, dv_name, strlen(dv_name)) == 0 )
        {
            /* Copy current information to old, and memset */
            strncpy( memory[i].odev_name, memory[i].dev_name,
                     strlen(memory[i].dev_name) );
            strncpy( memory[i].omqtt_topic, memory[i].mqtt_topic,
                     strlen(memory[i].mqtt_topic) );
            memset( memory[i].dev_name, 0, DB_DATA_LEN );
            memset( memory[i].mqtt_topic, 0, DB_DATA_LEN );

            /* unsubscribe from this device */
            mqtt_unsubscribe( cl, memory[i].stat_topic );

            /* add this device to database! */
            to_change[i] = 6;

            /* set return value to success */
            rv = 0;

            /* all done */
            break;
        }
    }

    pthread_mutex_unlock( lock );
    sem_post( mutex );

    return rv;
}


/**
 * @brief Function that prints devices in memory when requested to list
 * devices by client.
 *
 * @param fd the socket fd of the client.
 * @param n the clien's buffer number
 *
 * @note This uses semaphores since it access memory to satisfy the request.
 *
 */
static void dump_devices( int fd, const int n )
{
    sem_wait( mutex );
    pthread_mutex_lock( lock );

    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        /* Empty, move on */
        if ( memory[i].dev_name[0] == '\0' )
        {
            continue;
        }

        char *dv_type = device_type_to_str( memory[i].dev_type );
        int response_len = (10 + strlen(memory[i].dev_name) +
                  strlen(memory[i].mqtt_topic) + strlen(dv_type));
        snprintf( server_buffer[n], response_len, "%s -- %s -- %s\n",
                  memory[i].dev_name, memory[i].mqtt_topic, dv_type );

        int status = write( fd, server_buffer[n], response_len );

        /* client must have disconnected, move on */
        /* set equal to 0 if bugs come up! */
        if ( status <= 0 )
        {
            close( fd );
            fd = -1;
            break;
        }
    }

    pthread_mutex_unlock( lock );
    sem_post( mutex );
}

/**
 * @brief Create, Initialize, and return the server's socketfd.
 *
 * @param port the port the server listens on.
 *
 * @note Returns -1 if an error occurs, the actual integer otherwise.
 */
int create_server_socket( const int port )
{
    int fd;
    struct sockaddr_in serv_addr;

    /* actually create the socket here. */
    fd = socket( AF_INET, SOCK_STREAM, 0 );

    if ( fd < 0 )
    {
        perror( "error creating socket" );
        log_error( "error creating socket" );
        return -1;
    }

    /* set serv_addr to 0 initially. */
    memset( &serv_addr, 0, sizeof(serv_addr) );

    serv_addr.sin_family = AF_INET;

    /* allow anyone and anything to connect .. for now. */
    serv_addr.sin_addr.s_addr = htonl( INADDR_ANY );
    //inet_pton( AF_INET, IP_ADDR, &serv_addr.sin_addr );

    log_debug( "using port %u for server", (port & 0xffff) );
    serv_addr.sin_port = htons( port & 0xffff );

    /* Bind the fd */
    if ( bind( fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr) ) < 0 )
    {
        perror( "Error binding" );
        log_error( "Error binding server socket" );
        fd = -1;
    }

    /* Return the completed process. */
    return fd;
}

/**
 * @brief the server's connection handler
 *
 * @param connfds the pollfd array of clients.
 * @param num the count of clients in the pollfd array.
 *
 */
static void server_connection_handler( struct pollfd *connfds, const int num )
{
    int count; /* To handle connections */
    int n = 0; /* get the length of read() and write() functions */
    int response_len = 0; /* the server's response length to client */
    int status = 0; /* Status according to what server client wants */

    for ( count = 1; count <= num; count++ )
    {
        /* No connection, move on */
        if ( connfds[count].fd < 0 )
        {
            continue;
        }

        if ( connfds[count].revents & POLLIN )
        {
            /* Retreive request from client */
            n = read( connfds[count].fd, server_buffer[count - 1],
                      conf->buffer_size );

            /* client must have disconnected, move on */
            /* set equal to 0 if bugs come up! */
            if ( n <= 0 )
            {
                close( connfds[count].fd );
                connfds[count].fd = -1;
                continue;
            }

            /* Parse incoming request */
            status = parse_server_request( server_buffer[count - 1],
                                           &response_len );

            /* Write response to client */
            n = write( connfds[count].fd, server_buffer[count - 1],
                       response_len );

            /* Handle exit if client wants to exit */
            if ( status < 0 )
            {
                close( connfds[count].fd );
                connfds[count].fd = -1;
            }
            /* the LIST request was sent */
            else if ( status )
            {
                dump_devices( connfds[count].fd, (count - 1) );
            }
        }
    }
}

/**
 * @brief the server's network loop
 *
 * @param listenfd the socket created using
 * the function create_server_socket(port)
 *
 * @note Returns 1 for any errors that
 * may arise, returns 0 otherwise.
 */
int server_loop( const int listenfd )
{
    int rv = 0; /* return value, assume all is well */
    int connfd; /* the client's fd if one is accepted */
    int maxi = 0;
    int nready;
    int count;
    struct sockaddr_in cli_addr;
    socklen_t cli_addr_len = sizeof(cli_addr);

    /**
     * @todo ... maybe?
     * This may have to be malloc'd at some point, we'll
     * have to wait and see
     */
    struct pollfd clientfds[POLL_SIZE];

    /* first client is the server itself. */
    clientfds[0].fd = listenfd;
    clientfds[0].events = POLLIN;

    /* Initialize all other clients. */
    for ( int i = 1; i < POLL_SIZE; i++ )
    {
        clientfds[i].fd = -1;
    }

    /* run forever! */
    for ( ;; )
    {
        /* Wait for file descriptor to be ready in 50ms */
        nready = poll( clientfds, maxi+1, 50 );

        if ( nready < 0 )
        {
            log_error( "Error polling in server" );
            rv = 1;
            break;
        }

        if ( clientfds[0].revents & POLLIN )
        {
            /* Accept some clients! */
            if ( (connfd = accept(listenfd, (struct sockaddr*)&cli_addr,
                                  &cli_addr_len)) < 0 )
            {
                /*
                 * Handle potential interrupt beyond our control,
                 * and just start over again.
                 */
                if ( errno == EINTR )
                {
                    log_info( "Got interrupted with EINTR, continuing." );
                    continue;
                }
                else
                {
                    log_error( "Error Accepting client in server" );
                    rv = 1;
                    break;
                }
            }

            log_info( "Accepted new client %s:%d",
                      inet_ntoa(cli_addr.sin_addr),
                      cli_addr.sin_port );

            /* Add the client to next available clientfds slot. */
            for ( count = 1; count < POLL_SIZE; count++ )
            {
                if ( clientfds[count].fd < 0 )
                {
                    clientfds[count].fd = connfd;
                    break;
                }

                /*
                 * Made it this far into the loop,
                 * increment count, there are too many clients.
                 * Handle accordingly.
                 */
                if ( count == (POLL_SIZE - 1) )
                {
                    count++;
                }
            }

            /*
             * If there are too many clients, tell the
             * interested party that there are too many
             * clients right now.
             */
            if ( count >= POLL_SIZE )
            {
                log_warn( "Too many clients reached!" );
                write( connfd, FULL_MESSAGE, FULL_MESSAGE_LEN );
                close( connfd );
            }

            /* Set the clientfds as poll input. */
            clientfds[count].events = POLLIN;

            /* set or reset maxi if count is greater than it. */
            maxi = ( count > maxi ? count : maxi );

            if ( --nready <= 0 )
            {
                continue;
            }
        }

        /* handle the connection */
        server_connection_handler( clientfds, maxi );

        /* If it's exit time, exit cleanly */
        if ( closeSocket > 0 )
        {
            rv = 0;
            close( clientfds[0].fd );
            clientfds[0].fd = -1;
            break;
        }
    }

    return rv;
}

/* ability to close the socket */
void close_socket()
{
    closeSocket = 1;
}

/*******************************************************************************
 * Everything related to mqtt will reside here.
 ******************************************************************************/

/**
 * @brief Create a non-blocking socket for mqtt use.
 *
 * @param addr The address of an mqtt broker server.
 * @param port The port the mqtt broker server listens on.
 *
 * @note Returns -1 if an error occurs, the actual integer otherwise.
 */
int open_nb_socket( const char *addr, const char *port )
{
    struct addrinfo hints = {0};

    hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Must be TCP */
    int sockfd = -1;
    int rv;
    struct addrinfo *p, *servinfo;

    /* get address information */
    log_debug( "using port %s for mqtt", port );
    rv = getaddrinfo(addr, port, &hints, &servinfo);
    if ( rv != 0 )
    {
        fprintf(stderr, "Failed to open socket (getaddrinfo): %s\n",
        gai_strerror(rv));
        log_error( "Failed to open socket (getaddrinfo): %s\n",
        gai_strerror(rv) );
        return -1;
    }

    /* open the first possible socket */
    for ( p = servinfo; p != NULL; p = p->ai_next )
    {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if ( sockfd == -1 )
        {
            continue;
        }

        /* connect to server */
        rv = connect(sockfd, p->ai_addr, p->ai_addrlen);
        if ( rv == -1 )
        {
          close(sockfd);
          sockfd = -1;
          continue;
        }
        break;
    }

    /* free servinfo */
    freeaddrinfo(servinfo);

    /* make non-blocking */
    if ( sockfd != -1 )
    {
        fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
    }
    /* return the new socket fd */
    return sockfd;
}

/**
 * @brief The Mqtt init function.
 *
 * @param client the mqtt_client struct.
 * @param sockfd the mqtt server's sockfd.
 * @param snd_buf the mqtt client's send buffer.
 * @param recv_buf the mqtt client's receive buffer.
 * @param conf the configuration struct.
 *
 * @note returns 1 for when an error occurs, 0 otherwise.
 */
int initialize_mqtt( struct mqtt_client *client, int *sockfd,
                               uint8_t *snd_buf, uint8_t *recv_buf,
                               config *conf )
{
    mqtt_init( client, *sockfd, snd_buf, conf->snd_buff,
               recv_buf, conf->recv_buff, publish_kl_callback );

    /* Create an anonymous session */
    const char* client_id = NULL;

    /* Ensure we have a clean session */
    uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;

    /* Send connection request to the broker. */
    mqtt_connect( client, client_id, NULL, NULL, 0, NULL, NULL,
    connect_flags, KEEP_ALIVE );

    /* check that we don't have any errors */
    if (client->error != MQTT_OK) {
        log_error( "mqtt error: %s", mqtt_error_str(client->error) );

        return 1;
    }

    cl = client;

    return 0;
}

/**
 * @brief The mqtt publish (when subscribed) callback function.
 *
 * @param client not used
 * @param published contains topic_name and application_message
 *
 */
void publish_kl_callback( void** client,
                         struct mqtt_response_publish *published )
{
    /*
     * If a device state changes, update it accordingly
     */
    sem_wait( mutex );
    pthread_mutex_lock( lock );

    int topic_found = -1;
    int memory_location = -1;

    strncpy( topic, published->topic_name, published->topic_name_size );
    strncpy( app_msg, published->application_message,
             published->application_message_size );
    printf( "%s : %s\n", topic, app_msg );

    /* Find a match! */
    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        if ( strncasecmp(topic, memory[i].stat_topic,
                         strlen(topic)) == 0 )
        {
            topic_found = 1;
            memory_location = i;
            break;
        }

        /* no matching topics? for now just quietly ignore. */
        if ( i == (conf->max_dev_count - 1) )
        {
            topic_found = 0;
        }
    }

    /* if a match is found, change the state */
    if ( topic_found )
    {
        /* assume the dev_state will change */
        int change_permitted = 1;

        /*
         * Now to compare app message
         */
        if ( strncasecmp(app_msg, "ON", 3) == 0 )
        {
            memory[memory_location].dev_state = 1;
        }
        else if ( strncasecmp(app_msg, "OFF", 4) == 0 )
        {
            memory[memory_location].dev_state = 0;
        }
        else
        {
            /* If here is reached, don't bother for now */
            /*
             * THIS MAY NEED TO BE REVISED
             * TO SUPPORT RGB LIGHTS... IF
             * I (Christian) DECIDE TO SUPPORT THOSE.
             */
            log_warn( "Detected something different from topic %s : %s",
                      topic, app_msg );
            change_permitted = 0;
        }

        if ( change_permitted )
        {
            /* make sure a change isn't already staged */
            switch( to_change[memory_location] )
            {
                // Only state needs to be updated, continue
                case -1:
                case 0:
                    to_change[memory_location] = 0;
                    break;

                // dev_name is to be updated, stage as a full update
                case 1:
                {
                    strncpy( memory[memory_location].omqtt_topic,
                             memory[memory_location].mqtt_topic,
                             strlen(memory[memory_location].mqtt_topic) );

                    to_change[memory_location] = 4;
                    break;
                }

                // mqtt_topic is to be updated, stage a full update
                case 2:
                {
                    strncpy( memory[memory_location].odev_name,
                             memory[memory_location].dev_name,
                             strlen(memory[memory_location].dev_name) );

                    to_change[memory_location] = 4;
                    break;
                }

                // dev_type is to be updated, stage a full update
                case 3:
                {
                    strncpy( memory[memory_location].odev_name,
                             memory[memory_location].dev_name,
                             strlen(memory[memory_location].dev_name) );
                    strncpy( memory[memory_location].omqtt_topic,
                             memory[memory_location].mqtt_topic,
                             strlen(memory[memory_location].mqtt_topic) );

                    to_change[memory_location] = 4;
                    break;
                }

                // No need to update these
                case 4:
                case 5:
                case 6:
                    /* ignore, leave value alone */
                    break;

                default:
                {
                    log_error( "Reached the default case which" \
                               " shouldn't happen" );
                    break;
                }
            }
        }
    }

    /* memset topic and app message buffers for later use */
    memset( topic, 0, conf->topic_buff );
    memset( app_msg, 0, conf->app_msg_buff );

    /* All done! (for now) */
    pthread_mutex_unlock( lock );
    sem_post( mutex );
}

/**
 * @brief the refresher which sends/receives mqtt responses
 * at every roughly 100ms
 *
 * @param client is the struct mqtt_client to pass in.
 */
void *client_refresher(void* client)
{
    while(1)
    {
        mqtt_sync( (struct mqtt_client*) client );

        usleep( 100000U );
    }
    return NULL;
}
