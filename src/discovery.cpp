/*
 * This is a multicast approach to 
 * discovery.
 * 
 * The general idea here is to basically
 * implement something similar to SSDP, but
 * majorly stripped down, and purely for
 * discovery, nothing advanced.
 * 
 * Written by: Christian Kissinger
 */

// System-related includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>

// Socket-related includes
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// local includes
#include "discovery.h"
#include "server.h"
#include "common.h"
#include "log.h"

/*
 * This has the simple task of returning a UUID,
 * or generate one if one does not exist.
 * 
 * It should be noted that this will generate
 * a UUID Version 4.
 * Reference - http://www.ietf.org/rfc/rfc4122.txt
 * 
 */
static uint8_t get_uuid( char *uuid )
{
    unsigned char hexOctet[16];

    /*
     * Open a file at a particular location.
     * And check if it even contains anything.
     */
    FILE *uuid_file = fopen( UUID_LOCATION, "r" );

    /* if no file exists, create it! */
    if ( uuid_file == NULL )
    {
        uuid_file = fopen( UUID_LOCATION, "w" );

        /* However, if it is still null by this point, we're in trouble. */
        if ( uuid_file == NULL )
        {
            fprintf( stderr, "Unable to open file %s: might be a permission issue.\n",
                     UUID_LOCATION );
            
            /* Clean up after ourselves here. */
            fclose( uuid_file );

            return EXIT_FAILURE;
        }
    }

    /* Scan for the uuid */
    fscanf( uuid_file, "%s", uuid );

    /* Check if one exists */
    /* Otherwise we will generate one, and only once! */
    if ( uuid[0] == '\0' && uuid[1] == '\0' && uuid[2] == '\0' )
    {
        printf( "Generating UUID\n" );

        /* Initialize seed */
        srand( (unsigned int) time(NULL) );

        /* Populate hexOctet array */
        for ( int i = 0; i < 16; i++ )
        {
            hexOctet[i] = (unsigned char)rand() % 255;
        }

        /* Attempt to create a timestamp */
        hexOctet[6] = 0x40 | (hexOctet[6] & 0xf);
        hexOctet[8] = 0x80 | (hexOctet[8] & 0x3f);
        

        sprintf( uuid, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
         hexOctet[0], hexOctet[1], hexOctet[2], hexOctet[3], hexOctet[4],
         hexOctet[5], hexOctet[6], hexOctet[7], hexOctet[8], hexOctet[9],
         hexOctet[10], hexOctet[11], hexOctet[12], hexOctet[13],
         hexOctet[14], hexOctet[15] );
        
         /* Write to file so we don't do this again! */
         fprintf( uuid_file, "%s\n", uuid );
    }

    /* Clean up after ourselves here. */
    fclose( uuid_file );

    return EXIT_SUCCESS;
}

/*
 * Analyze packet request.
 * 
 * returns 1 to indicate that it is
 * "not for me",
 * 
 *  and returns 0 for it is "for me",
 *  and to send a response to whom
 *  asked.
 */
static uint8_t parse_buffer( char *buf )
{
    /* for sscanf's usage 
     * shouldn't ever have more than
     * two arguments to analyze anyways.
     */
    char temp_buf[2][256];

    /* return value, assume it is not "for me". */
    uint8_t rv = 1;

    /* scan first input */
    sscanf( buf, "%s", temp_buf[0] );

    if ( strcmp(temp_buf[0], "WHOHAS") == 0 )
    {
        sscanf( buf, "%s %s", temp_buf[0], temp_buf[1] );

        /* If we find a match, it is okay to send
         * a response packet.
         */
        if ( strcmp(temp_buf[1], "*") == 0  || 
             strcmp(temp_buf[1], SERVICE_NAME) == 0 )
        {
            rv = 0;
        }
        else
        {
            rv = 1;
        }
    }

    return rv;
}

/*
 * Create discovery socket,
 * receive a discovery packet and
 * then send a response!
 */
static uint8_t discovery_loop( char *uuid )
{
    /* sk for socket descriptor, socket option flag, 
     * received buffer length
     */
    int sk; 
    int flag_on = 1; 
    int n;

    /* req_addr length */
    unsigned int req_len;

    /*
     * Socket address structure,
     * and one for incoming packet sources.
     */
    struct sockaddr_in m_addr;
    struct sockaddr_in req_addr;
    
    /*
     * buffer string
     */
    char buf[BUFFER_SIZE + 1];   

    /*
     * Multicast request structure
     */
    struct ip_mreq m_req;

    /* create socket to set multicast group upon */
    if ( (sk = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 )
    {
        fprintf( stderr, "Unable to create discovery socket.\n" );
        return 1;
    }

    /* Allow multiple binds per host by setting reuse port flag to on. */
    if ( (setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, 
          &flag_on, sizeof(flag_on))) < 0 )
    {
        fprintf( stderr, "Unable to set socket option, cannot reuse port!\n" );
        return 1;
    }

    /* setup a multicast address structure */
    memset( &m_addr, 0, sizeof(m_addr) );
    m_addr.sin_family = AF_INET;
    m_addr.sin_addr.s_addr = htonl( INADDR_ANY );
    m_addr.sin_port = htons( DISCOVERY_PORT );

    /* bind to socket */
    if ( (bind(sk, (struct sockaddr *) &m_addr,
          sizeof(m_addr))) < 0 )
    {
        fprintf( stderr, "Unable to bind sock.\n" );
        return 1;
    }

    /* setup an IGMP join request structure */
    m_req.imr_multiaddr.s_addr = inet_addr( ADV_IP_ADDR );
    m_req.imr_interface.s_addr = htonl( INADDR_ANY );

    /* send an ADD MEMBERSHIP request via setsockopt */
    if ( (setsockopt(sk, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
         (void *) &m_req, sizeof(m_req))) < 0 )
    {
        fprintf( stderr, 
        "Unable to set socket option, cannot join membership of IGMP.\n" );
        return 1;
    }

    /* Now the loop begins! */
    for ( ;; )
    {
        /* clear the buffer and struct(s) */
        memset( buf, 0, sizeof(buf) );
        req_len = sizeof( req_addr );
        memset( &req_addr, 0, req_len );

        /* wait for an incoming request packet */
        if ( (n = recvfrom(sk, buf, BUFFER_SIZE, 0,
             (struct sockaddr *) &req_addr, &req_len)) < 0)
        {
            fprintf( stderr, "recvFrom failed, bailing out.\n" );
            return 1;
        }
        
        printf( "received %d bytes from %s: ", n,
                inet_ntoa(req_addr.sin_addr) );
        printf( "%s", buf );

        /* Analyze sent data */

        /* if true, listen for more requests. */
        if ( parse_buffer(buf) )
        {
            continue;
        }

        /* send a response */
        sprintf( buf, "UUID: %s\nservice: %s\nport: %d\nip: ",
                 uuid, SERVICE_NAME, PORT );
        
        n = strlen( buf );

        /* direct response */
        if ( (sendto(sk, buf, n, 0,
             (struct sockaddr *) &req_addr, sizeof(req_addr))) != n )
        {
            fprintf( stderr, "sendto() direct sent incorrect number of bytes\n" );
            return 1;
        }
    }

    /* send a DROP MEMBERSHIP request via setsockopt */
    if ( (setsockopt(sk, IPPROTO_IP, IP_DROP_MEMBERSHIP,
         (void *) &m_req, sizeof(m_req))) < 0 )
    {
        fprintf( stderr, "Unable to set socket option, cannot drop membership of IGMP.\n"  );
        return 1;
    }

    /* close our socket */
    close( sk );

    return 0;
}

void *discovery_handler( void *varg )
{
    /* Initialize UUID char string */
    char uuid[16];
    memset( uuid, '\0', sizeof(char)*16 );

    uint8_t status = get_uuid( uuid );

    if ( status != 0 )
    {
        printf( "UUID not found, discovery service cannot be used.\n" );

        pthread_exit( &status );
    }

    /* Create discovery socket and start discovery loop */
    status = discovery_loop( uuid );

    if ( status != 0 )
    {
        printf( "Error occured in the discovery_loop function, bailing out.\n" );

        pthread_exit( &status );
    }

    pthread_exit( &status );
}