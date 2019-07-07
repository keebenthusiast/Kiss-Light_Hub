/*
 *  General and miscellaneous functions
 *  will reside here, for ease.
 * 
 *  Written by: Christian Kissinger
 */

// system-related includes
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// local includes
#include "common.h"
#include "log.h"
#include "RCSwitch.h"

/* Get current date and time, useful for the logger */
void get_current_time( char *buf )
{
    time_t now = time(0);
    struct tm tstruct;

    tstruct = *localtime( &now );

    strftime( buf, sizeof(buf)+24, "%Y-%m-%d.%X", &tstruct );
}

/* 
 * Parse whatever the client sent in,
 * Do the task if applicable,
 * And send in the proper response.
 * 
 * returns -1 to exit,
 * returns 0 for all is good,
 * returns 1 to enter sniff mode,
 */
int parse_input( char *buf, int *n )
{
    char lgbuf[256];
    char str[4][64];
    int rv = 0; 

    sscanf( buf, "%s", str[0] );

    if ( strcmp(str[0], "TRANSMIT") == 0 )
    {
        int c, p;
        sscanf( buf, "%s %i %i", str[0], &c, &p );

        send_rf_signal( c, p );
        sprintf( lgbuf,"entered Code: %i Pulse: %i", c, p );
        write_to_log( lgbuf );

        /* All done, write the response to the buffer. */
        *n = sprintf( buf, "KL/0.1 200 Custom Code Sent\n" );
    }
    else if ( strcmp(str[0], "TOGGLE") == 0 )
    {
        /* Not yet implemented */
        *n = sprintf( buf, "KL/0.1 200 Toggled\n" );
    }
    else if ( strcmp(str[0], "SNIFF") == 0 )
    {
        int code = 123, pulse = 189;
        *n = sprintf( buf, "KL/0.1 200 Sniffing\n" );
        rv = 1;

    }
    else if ( strcmp(str[0], "Q") == 0 || strcmp(str[0], "QUIT") == 0 )
    {
        /* If user wants to exit, we'll let them know that their request is granted. */
        *n = sprintf( buf, "KL/0.1 200 Goodbye\n" );
        rv = -1;
    }
    else
    {
        *n = sprintf( buf, "KL/0.1 400 Bad Request\n" );
    }

    return rv;
}

/* send RF Signal */
void send_rf_signal( int code, int pulse )
{
    /* toggle switch, given code and pulse */
    RCSwitch sw = RCSwitch();
    sw.RCSwitch::setPulseLength( pulse );

    /* transmit is PIN 0 according to wiringPi */
    /*
     * TODO:
     *  Make this customizable potentially
     */
    sw.RCSwitch::enableTransmit( 0 );

    /* so 24 is apparently the length of the signal, modify-able? */
    sw.RCSwitch::send( code, 24 );
}

/* sniff RF signal from a remote (for example) */
void sniff_rf_signal( int &code, int &pulse )
{
    RCSwitch sw = RCSwitch();
    
    /* receive (sniff) is PIN 2 according to wiringPi */
    /*
     * TODO:
     *  Make this customizable potentially
     */
    sw.RCSwitch::enableReceive( 2 );

    /*
     * Wait until something is found,
     * once found, exit as it's done.
     */
    while ( 1 )
    {
        if ( sw.RCSwitch::available() )
        {
            code = sw.RCSwitch::getReceivedValue();
            pulse = sw.RCSwitch::getReceivedDelay();
            break;
        }

        sw.RCSwitch::resetAvailable();
    }
}