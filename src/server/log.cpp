/*
 * This is the log file, which should save the logfile as
 * "/var/log/kisslight.log"
 * and it should contain information from both the server 
 * and rf transmitter, and errors within those and such.
 * 
 * Written by: Christian Kissinger
 */

// system-related includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

// local includes
#include "common.h"
#include "log.h"

/* Initialize log */
void initialize_logger()
{
    FILE *lg = fopen( LOGLOCATION, "w" );
    
    if ( lg == NULL )
    {
        perror( "Error: " );
        exit( 1 );
    }

    char str[32];
    get_current_time( str );

    fprintf( lg, "kisslight log started at %s \n", str );

    fclose( lg );
}

/* 
 * Write basically anything to log,
 * no need to add a "\n" to the end of the string.
 */
void write_to_log( char *str )
{
    FILE *lg = fopen( LOGLOCATION, "a" );

    char tm[32];
    get_current_time( tm );

    fprintf( lg, "%s - %s\n", tm, str );

    fclose( lg );
}