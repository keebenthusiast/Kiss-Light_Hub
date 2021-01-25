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

/* Local prototypes as needed */

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
 * returns 0 for all is good.
 */
int parse_server_request(char *buf, int *n)
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
        *n = snprintf( buf, 32, "KL/%.1f 407 Not Yet Implemented\n",
                       KL_VERSION );
    }
    // TOGGLE dev_name KL/version#
    else if ( strncasecmp(str_buffer[0], "TOGGLE", 7) == 0 )
    {
        *n = snprintf( buf, 32, "KL/%.1f 407 Not Yet Implemented\n",
                       KL_VERSION );
    }
    // SET dev_name [ON,1|OFF,0] KL/version#
    else if ( strncasecmp(str_buffer[0], "SET", 4) == 0 )
    {
        *n = snprintf( buf, 32, "KL/%.1f 407 Not Yet Implemented\n",
                       KL_VERSION );
    }
    // LIST KL/version#
    else if ( strncasecmp(str_buffer[0], "LIST", 5) == 0 )
    {
        *n = snprintf( buf, 32, "KL/%.1f 407 Not Yet Implemented\n",
                       KL_VERSION );
    }
    // DELETE dev_name KL/version#
    else if ( strncasecmp(str_buffer[0], "DELETE", 7) == 0 )
    {
        *n = snprintf( buf, 32, "KL/%.1f 407 Not Yet Implemented\n",
                       KL_VERSION );
    }
    // STATUS dev_name KL/version#
    else if ( strncasecmp(str_buffer[0], "STATUS", 7) == 0 )
    {
        *n = snprintf( buf, 32, "KL/%.1f 407 Not Yet Implemented\n",
                       KL_VERSION );
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
static void server_connection_handler( struct pollfd *connfds, int num )
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
                continue;
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
int server_loop( int listenfd )
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
            printf( "Detected that it is on!\n" );
            memory[memory_location].dev_state = 1;
        }
        else if ( strncasecmp(app_msg, "OFF", 4) == 0 )
        {
            printf( "Detected that it is off!\n" );
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
