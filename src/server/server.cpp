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

// socket-related includes
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>

// electronics-related includes
#include <wiringPi.h>

// local includes
#include "server.h"
#include "log.h"
#include "common.h"
#include "daemon.h"

int main( int argc, char **argv )
{
    int listenfd;

    /* 
     * Initialize Configuration file
     */
    initialize_conf_parser();

    /*
     * Initialize Logger
     */
    initialize_logger();

    /*
     * Run as Daemon if specified.
     */
    
    if ( argc >= 2 )
    {
        if ( strcmp(argv[1], "daemon") == 0 )
        {
            fprintf( stdout, "running as daemon\n" );
            run_as_daemon();

            /* Daemon will handle the one signal */
            signal( SIGINT, handle_signal );
        }
    }

     /*
     * Initialize wiringPi,
     * and RCSwitch.
     */
    if ( wiringPiSetup() < 0 )
    {
        perror( "wiringPi error" );
        write_to_log( (char *)"error initializing WiringPi, insufficient privileges" );
        return -1;
    }
    write_to_log( (char *)"WiringPi initialized successfully" );

    initialize_rc_switch();
    
    /*
     * Create socket.
     */
    listenfd = create_socket();

    /*
     * start listening.
     */
    if ( listen( listenfd, LISTEN_QUEUE ) < 0 )
    {
        perror( "Listening error" );
        write_to_log( (char *)"error listening" );
        return -1;
    }

    /*
     * Time for Polling.
     */
    network_loop( listenfd );

    return 0;
}


/* Create, Initialize, and return the socketfd. */
static int create_socket()
{
    int fd;
    struct sockaddr_in serv_addr;
    char lgbuf[LOG_BUFFER_SIZE];

    /* actually create the socket here. */
    fd = socket( AF_INET, SOCK_STREAM, 0 );

    if ( fd < 0 )
    {
        perror( "error creating socket" );
        write_to_log( (char *)"error creating socket" );
        exit( 1 );
    }

    /* set serv_addr to 0 initially. */    
    memset( &serv_addr, 0, sizeof(serv_addr) );

    serv_addr.sin_family = AF_INET;

    /* allow anyone and anything to connect .. for now. */
    serv_addr.sin_addr.s_addr = htonl( INADDR_ANY );
    //inet_pton( AF_INET, IP_ADDR, &serv_addr.sin_addr );

    sprintf( lgbuf,"using port %u", get_int("network", "port", PORT) & 0xffff );
    write_to_log( lgbuf );
    serv_addr.sin_port = htons( get_int("network", "port", PORT) & 0xffff );

    /* Bind the fd */
    if ( bind( fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr) ) < 0 )
    {
        perror( "Error binding" );
        write_to_log( (char *)"error binding" );
        exit( 1 );
    }

    /* Return the completed process. */
    return fd;
}

static void connection_handler( struct pollfd *connfds, int num )
{
    int i, n, status;
    char buf[num][BUFFER_SIZE];
    memset( buf, 0, BUFFER_SIZE );

    for ( i = 1; i <= num; i++ )
    {
        if ( connfds[i].fd < 0 )
            continue;
        
        if ( connfds[i].revents & POLLIN )
        {
            n = read( connfds[i].fd, buf[i-1], BUFFER_SIZE );

            if ( n == 0 )
            {
                close( connfds[i].fd );
                connfds[i].fd = -1;
                continue;
            }

            status = parse_input( buf[i-1], &n );

            write( connfds[i].fd, buf[i-1], n );

            /* Handle sniff request */
            if ( status == 1 )
            {
                int code, pulse;
                sniff_rf_signal( code, pulse );

                if ( code <= 0 )
                {
                    n = sprintf( buf[i-1], "KL/0.1 404 Unknown Encoding\n" );
                }
                else
                {
                    n = sprintf( buf[i-1], "KL/0.1 200 Code: %i Pulse: %i\n",
                                 code, pulse );
                }

                write( connfds[i].fd, buf[i-1], n );
            }

            /* Handle exit if client wants to exit */
            if ( status < 0 )
            {
                close ( connfds[i].fd );
                connfds[i].fd = -1;
                continue;
            }
        }
    }
}

static void network_loop( int listenfd )
{
    int connfd, maxi = 0, nready, i;
    char lgbuf[LOG_BUFFER_SIZE];
    struct sockaddr_in cli_addr;
    socklen_t cli_addr_len = sizeof(cli_addr);
    struct pollfd clientfds[POLL_SIZE];

    /* first client is the server itself. */
    clientfds[0].fd = listenfd;
    clientfds[0].events = POLLIN;

    /* Initialize all other clients. */
    for ( i = 1; i < POLL_SIZE; i++ )
    {
        clientfds[i].fd = -1;
    }

    /* go on forever! */
    for ( ;; )
    {
        /*
         * -1 means give unlimited time,
         * this may change..
         */
        nready = poll( clientfds, maxi+1, -1 );

        if ( nready < 0 )
        {
            perror( "Error Polling" );
            write_to_log( (char *)"error polling" );
            exit( 1 );
        }

        if ( clientfds[0].revents & POLLIN )
        {
            /* accept some clients! */
            if ( (connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_addr_len)) < 0 )
            {
                if ( errno == EINTR )
                {
                    continue;   
                }
                else
                {
                    perror( "Error Accepting" );
                    write_to_log( (char *)"error accepting" );
                    exit( 1 );
                }
            }

            /* output to stdout, and write to log as well. */
            fprintf( stdout, "accept new client: %s:%d\n", 
                                 inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port );
            sprintf( lgbuf,"accept new client: %s:%d", 
                                 inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port );
            write_to_log( lgbuf );

            /* Add the client to next available clientfds slot. */
            for ( i = 1; i < POLL_SIZE; i++ )
            {
                if ( clientfds[i].fd < 0 )
                {
                    clientfds[i].fd = connfd;
                    break;
                }
            }

            /* 
             * if there are too many clients, self-destruct!
             * 
             * Luckily not likely going to happen
             * with what I am working with...
             *  ... hopefully of course.
             */
            if ( i >= POLL_SIZE )
            {
                fprintf( stderr, "too many clients, exiting...\n" );
                write_to_log( (char *)"error, too many clients" );
                exit( 1 );
            }

            /* set the clientfds as poll input.. I believe. */
            clientfds[i].events = POLLIN;

            /* set or reset maxi if i is greater than it. */
            maxi = (i > maxi ? i : maxi);
            if ( --nready <= 0 )
                continue;
        }
        
        /* handle the actual connection */
        connection_handler( clientfds, maxi );
    }
}