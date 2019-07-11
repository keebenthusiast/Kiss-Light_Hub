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
#include <string>

// local includes
#include "common.h"
#include "log.h"
#include "ini.h"
#include "RCSwitch.h"
#include "INIReader.h"


/* local configuration variables */
static char strbuf[2048];
static INIReader *conf;
static RCSwitch *sw;

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

/*
 * Everything related to electrical will reside here. 
 */

/* Initialize configuration variable, to ini file */
void initialize_rc_switch()
{
    sw = new RCSwitch();
}

/* Clear out config variable, for a cleaner daemon exit. */
void stop_rc_switch()
{
    delete sw;
}

/* send RF Signal */
void send_rf_signal( int code, int pulse )
{
    /* toggle switch, given code and pulse */
    sw->setPulseLength( pulse );

    /* transmit is PIN 0 according to wiringPi */
    sw->enableTransmit( get_int("electronics", "transmitter_pin", 0) );

    /* so 24 is apparently the length of the signal, modify-able? */
    sw->send( code, 24 );
}

/* sniff RF signal from a remote (for example) */
void sniff_rf_signal( int &code, int &pulse )
{
    /* receive (sniff) is PIN 2 according to wiringPi */
    sw->enableReceive( get_int("electronics", "receiver_pin", 2) );

    /*
     * Wait until something is found,
     * once found, exit as it's done.
     */
    while ( 1 )
    {
        if ( sw->available() )
        {
            code = sw->getReceivedValue();
            pulse = sw->getReceivedDelay();
            break;
        }

        sw->resetAvailable();
    }
}

/*
 * Everything related to configuration will reside here. 
 */

/* Initialize configuration variable, to ini file */
void initialize_conf_parser()
{
    conf = new INIReader( CONFLOCATION );

    if ( conf->ParseError() < 0 )
    {
        fprintf( stdout, "Unable to find configuration file %s\n", CONFLOCATION );
        exit(1);
    }
}

/* Clear out config variable, for a cleaner daemon exit. */
void stop_conf_parser()
{
    delete conf;
}

/* Extract Integers from ini file. */
long get_int( const char *section, const char *name, long def_val )
{
    return conf->GetInteger( section, name, def_val );
}

/* Extract Strings from ini file. */
const char *get_string( const char *section, const char *name, const char *def_val )
{
    strcpy( strbuf, conf->GetString(section, name, def_val).c_str() );
    return strbuf;
}