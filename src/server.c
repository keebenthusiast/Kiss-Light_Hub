/*
 * A server whos job is given the command either "on" or "off",
 * it will turn on or off whatever is connected to the outlet.
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

/*
 * When exiting, close server's socket,
 * using this variable
 */
static unsigned int closeSocket = 0;

/*******************************************************************************
 * Everything related to message parsing will reside here.
 ******************************************************************************/

/*
 * Check protocol version
 * return protocol_version if valid
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

/*
 * Parse whatever the client sent in,
 * Do the task if applicable,
 * And send over the proper response.
 *
 * returns -1 to exit,
 * returns 0 for all is good,
 * returns 1 to enter sniff mode,
 */
int parse_server_input(char *buf, int *n)
{
    // not yet implemented
    return 1;
}

/*******************************************************************************
 * Everything related to the server will reside here.
 ******************************************************************************/

/* Create, Initialize, and return the server's socketfd. */
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

/* ability to close the socket */
void close_socket()
{
    closeSocket = 1;
}

/*******************************************************************************
 * Everything related to mqtt will reside here.
 ******************************************************************************/

/*
   Create a non-blocking socket for mqtt use!
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

/* The Mqtt init function */
int initialize_mqtt( struct mqtt_client *client, int sockfd,
                               uint8_t *snd_buf, uint8_t *recv_buf,
                               config *conf )
{
    mqtt_init( client, sockfd, snd_buf, conf->snd_buff,
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

    return 0;
}

/* The publish callback function */
void publish_kl_callback(void** unused, struct mqtt_response_publish *published)
{
    /* not used in this example */
}

/**
 * @brief the refresher which sends/receives mqtt responses
 * at every roughly 100ms
 *
 * @param client is the struct mqtt_client to pass in.
 */
void* client_refresher(void* client)
{
    while(1)
    {
        mqtt_sync((struct mqtt_client*) client);
        usleep(100000U);
    }
    return NULL;
}
