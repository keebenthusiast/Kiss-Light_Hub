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
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

// socket-related includes
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>

// electronics-related includes
#include <pigpio.h>

// local includes
#include "server.h"
#include "log.h"
#include "common.h"
#include "daemon.h"
#include "discovery.h"

/* 
 * When exiting, close server's socket,
 * using this variable
 */
static unsigned int closeSocket = 0;

/*
 * For purposes of sniffing.
 */
struct sniffer
{
    int code;
    int pulse;
    int timeout;
};

// more server-specific templates
static int create_socket();
static void connection_handler(struct pollfd *connfds, int num);
static void network_loop(int listenfd);
static void *smalloc( size_t size );
static int sfree( void *smlc, size_t size );

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
        }
    }

    /* Daemon will handle the one signal */
    signal( SIGINT, handle_signal );

    /*
     * Initialize pigpio,
     * RCSwitch, and Status LEDs.
     */
    
    if ( gpioInitialise() < 0 )
    {
        perror( "pigpio error" );
        write_to_log( (char *)"error initializing pigpio, insufficient privileges" );
        return -1;
    }
    write_to_log( (char *)"pigpio initialized successfully" );

    initialize_rc_switch();
    initialize_leds();
    
    /*
     * Create discovery thread,
     */
    pthread_t discov;
    pthread_create( &discov, NULL, discovery_handler, NULL );

    /*
     * Create socket.
     */
    listenfd = create_socket();

    /*
     * start listening.
     */
    if ( listen( listenfd, LISTEN_QUEUE ) < 0 )
    {
        set_status_led( PI_HIGH, PI_HIGH, PI_HIGH );
        perror( "Listening error" );
        write_to_log( (char *)"error listening" );
        return -1;
    }

    /*
     * Time for Polling.
     */
    network_loop( listenfd );

    /* When this is reached, it is time to exit! */
    set_status_led( PI_LOW, PI_LOW, PI_LOW );
    gpioTerminate();

    return 0;
}


/* Create, Initialize, and return the socketfd. */
static int create_socket()
{
    int fd;
    struct sockaddr_in serv_addr;
    char lgbuf[get_int("log", "log_buffer_size", LOG_BUFFER_SIZE)];

    /* actually create the socket here. */
    fd = socket( AF_INET, SOCK_STREAM, 0 );

    if ( fd < 0 )
    {
        set_status_led( PI_HIGH, PI_HIGH, PI_HIGH );
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

    sprintf( lgbuf,"using port %u", (short)get_int("network", "port", PORT) & 0xffff );
    write_to_log( lgbuf );
    serv_addr.sin_port = htons( get_int("network", "port", PORT) & 0xffff );

    /* Bind the fd */
    if ( bind( fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr) ) < 0 )
    {
        set_status_led( PI_HIGH, PI_HIGH, PI_HIGH );
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
    char buf[num][get_int( "network", "buffer_size", BUFFER_SIZE )];
    memset( buf, 0, get_int("network", "buffer_size", BUFFER_SIZE) );

    for ( i = 1; i <= num; i++ )
    {
        if ( connfds[i].fd < 0 )
            continue;
        
        if ( connfds[i].revents & POLLIN )
        {
            n = read( connfds[i].fd, buf[i-1], get_int("network", "buffer_size", BUFFER_SIZE) );

            if ( n == 0 )
            {
                close( connfds[i].fd );
                connfds[i].fd = -1;
                continue;
            }

            status = parse_server_input( buf[i-1], &n );

            write( connfds[i].fd, buf[i-1], n );

            /* Handle sniff request */
            if ( status == 1 )
            {
                /* create shared memory pool first */
                struct sniffer *shdmem = (struct sniffer *)smalloc( sizeof( struct sniffer ) );
                shdmem->code = 0;
                shdmem->pulse = 0;
                shdmem->timeout = 0;

                /* fork process to accomplish this task */
                pid_t scn;
                scn = fork();

                /* Child process, execute! */
                if ( scn == 0 )
                {
                    /* retreive codes, if not timed out */
                    int code = 0, pulse = 0, timeout = 0;
                    sniff_rf_signal( code, pulse, timeout );
                    
                    /* copy values from child to shared memory */
                    shdmem->code = code;
                    shdmem->pulse = pulse;
                    shdmem->timeout = timeout;

                    /* terminate gpio for child, no longer needed */
                    gpioTerminate();

                    /* Child is finished yay */
                    exit( 0 );
                }
                /* Error occured while forking, tell client! */
                else if ( scn < 0 )
                {
                    /* Free the shared memory */
                    if ( sfree(shdmem, sizeof(struct sniffer)) < 0 )
                    {
                        write_to_log( (char *)"Error deleting memory after failed sniffing request" );
                        exit( 1 );
                    }

                    /* Tell client about this */
                    n = sprintf( buf[i-1], "KL/%.1f 500 unable to grant request\n", KL_VERSION );
                }
                /* Parent process, wait on child! */
                else
                {
                    /* Wait for child to complete */
                    wait( NULL );

                    /* Analyze code and pulse, and verify timeout didn't occur. */    
                    if( (shdmem->code <= 0 || shdmem->pulse <= 0) && shdmem->timeout <= 0 )
                    {
                        n = sprintf( buf[i-1], "KL/%.1f 406 Unknown Encoding\n", KL_VERSION );
                    }
                    else if ( shdmem->timeout > 0 )
                    {
                        n = sprintf( buf[i-1], "KL/%.1f 504 timed out\n", KL_VERSION );
                    }
                    else
                    {
                        n = sprintf( buf[i-1], "KL/%.1f 200 Code: %i Pulse: %i\n",
                                    KL_VERSION, shdmem->code, shdmem->pulse );
                    }
                    
                    /* Free the shared memory */
                    if ( sfree(shdmem, sizeof(struct sniffer)) < 0 )
                    {
                        write_to_log( (char *)"Error deleting memory after sniffing" );
                        exit( 1 );
                    }
                }

                write( connfds[i].fd, buf[i-1], n );
            }

            /* Handle exit if client wants to exit */
            if ( status < 0 )
            {
                close( connfds[i].fd );
                connfds[i].fd = -1;
                continue;
            }
        }
    }
}

static void network_loop( int listenfd )
{
    int connfd, maxi = 0, nready, i;
    char lgbuf[get_int("log", "log_buffer_size", LOG_BUFFER_SIZE)];
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
                    set_status_led( PI_HIGH, PI_HIGH, PI_HIGH );
                    perror( "Error Accepting" );
                    write_to_log( (char *)"error accepting" );
                    exit( 1 );
                }
            }

            /* output to stdout, and write to log as well. */
            fprintf( stdout, "accept new client: %s:%d\n", 
                                 inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port );
            sprintf( lgbuf, "accept new client: %s:%d", 
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
                set_status_led( PI_HIGH, PI_LOW, PI_HIGH );
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

        /* If it's exit time, exit cleanly */
        if ( closeSocket > 0 )
        {
            close( clientfds[0].fd );
            clientfds[0].fd = -1;
            break;
        }
    }
}

void close_socket()
{
    closeSocket = 1;
}

/*
 * Allows the server to create shared memory,
 * for usage when scanning/sniffing.
 */
void *smalloc( size_t size )
{
    /* Buffer will be readable and writable, shared but anonymous. */
    return mmap( NULL, size, (PROT_READ | PROT_WRITE), (MAP_SHARED | MAP_ANONYMOUS), -1, 0 );
}

/*
 * Shared memory can be freed for later use!
 * 
 * should return 0 for success, -1 for error.
 */
int sfree( void *smlc, size_t size )
{
    return munmap( smlc, size );
}
