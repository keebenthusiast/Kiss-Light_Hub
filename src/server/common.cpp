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
#include <sqlite3.h>
#include <string>

// electronics-related includes
#include <wiringPi.h>

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
static int LED0, LED1, LED2;

/* struct to save data from database, temporarily */
struct db_dev
{
    char *name[MAX_DEVICES];
    int on;
    int off;
    int pulse;
    int toggle;

    /*
     * Keep track number of names used,
     * for the LIST call.
     */
    int n = 0;
};
static struct db_dev db_access;
struct db_dev *db_ptr = &db_access;

/* So parse_server_input knows that these exist. */
static int dump_devices();
static int select_device( const char *name );
static int update_toggle( const char *name );
static int delete_device( const char *name );
static int add_device( const char *name, int on_code, int off_code, int pulse );

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
int parse_server_input( char *buf, int *n )
{
    char lgbuf[256];
    char str[4][256];
    int rv = 0;

    sscanf( buf, "%s", str[0] );
    // TRANSMIT 5592371 189 KL/version#
    if ( strcmp(str[0], "TRANSMIT") == 0 )
    {
        int c, p;
        sscanf( buf, "%s %i %i %s", str[0], &c, &p, str[1] );

        send_rf_signal( c, p );
        sprintf( lgbuf,"entered Code: %i Pulse: %i", c, p );
        write_to_log( lgbuf );

        /* All done, write the response to the buffer. */
        *n = sprintf( buf, "KL/0.1 200 Custom Code Sent\n" );
    }
    // TOGGLE outlet0 KL/version#
    else if ( strcmp(str[0], "TOGGLE") == 0 )
    {
        sscanf( buf, "%s %s", str[0], str[1] );

        int error = select_device( str[1] );

        /* We don't need the name currently, free it immediately. */
        if ( db_ptr->name[0] != NULL )
        {
            free( db_ptr->name[0] );
            db_ptr->name[0] = NULL;

            /* reset n too */
            db_ptr->n = 0;
        }

        if ( error == 0 )
        {
            if ( db_ptr->toggle )
            {
                send_rf_signal( db_ptr->on, db_ptr->pulse );
            }
            else
            {
                send_rf_signal( db_ptr->off, db_ptr->pulse );
            }
            sprintf( lgbuf,"entered Code: %i Pulse: %i", 
                     (db_ptr->toggle) ? db_ptr->on : db_ptr->off, db_ptr->pulse );
            write_to_log( lgbuf );

            *n = sprintf( buf, "KL/0.1 200 Device %s Toggled\n", str[1] );

            /* Toggle the toggle value in database. */
            error = update_toggle( str[1] );

            if ( error )
            {
                sprintf( lgbuf, "cannot set toggle value for %s", str[1] );
                write_to_log( lgbuf );
            }
            else
            {
                /* 
                 * All is well, do nothing ,
                 * or it could be an error
                 * related to being unable
                 * to open the database, either way,
                 * the log captures that.
                 */
            }
        }
        else if ( error < 0 )
        {
            *n = sprintf( buf, "KL/0.1 500 Internal Error\n" );
        }
        else
        {
            *n = sprintf( buf, "KL/0.1 406 Cannot Toggle Device %s\n", str[1] );
        }
    }
    // LIST KL/version#
    else if ( strcmp(str[0], "LIST") == 0 )
    {
        sscanf( buf, "%s %s", str[0], str[1] );

        int error = dump_devices();

        if ( error == 0 )
        {
            *n = sprintf( buf, "KL/0.1 200 Number of Devices %i\n", db_ptr->n );

            for ( int i = 0; i < db_ptr->n; i++ )
            {
                /* Add to buffer */
                strcat( buf, db_ptr->name[i] );
                strcat( buf, (char *)"\n" );
                *n += strlen( db_ptr->name[i] ) + 1;

                /* Free some strings */
                free( db_ptr->name[i] );
                db_ptr->name[i] = NULL;
            }

            db_ptr->n = 0;
        }
        else if ( error < 0 )
        {
            *n = sprintf( buf, "KL/0.1 500 Internal Error\n" );
        }
        else
        {
            *n = sprintf( buf, "KL/0.1 406 Cannot Retreive Devices\n" );
        }
    }
    // ADD 'name' on_code off_code pulse KL/Version#
    // ADD outlet0 5592371 5592380 189 KL/version#
    else if ( strcmp(str[0], "ADD") == 0 )
    {
        int on, off, pulse, error;
        sscanf( buf, "%s %s %i %i %i %s",
                str[0], str[1], &on, &off, &pulse, str[2] );

        error = add_device( str[1], on, off, pulse );

        if ( error == 0 )
        {
             *n = sprintf( buf, "KL/0.1 200 Device %s Added\n", str[1] );
        }
        else if ( error < 0 )
        {
            *n = sprintf( buf, "KL/0.1 500 Internal Error\n");
        }
        else
        {
            *n = sprintf( buf, "KL/0.1 406 Cannot Add Device %s\n", str[1] );
        }
    }
    // DELETE 'name' KL/Version#
    else if ( strcmp(str[0], "DELETE") == 0 )
    {
        sscanf( buf, "%s %s", str[0], str[1] );

        int error = delete_device( str[1] );

        if ( error == 0 )
        {
             *n = sprintf( buf, "KL/0.1 200 Device %s Deleted\n", str[1] );
        }
        else if ( error < 0 )
        {
            *n = sprintf( buf, "KL/0.1 500 Internal Error\n");
        }
        else
        {
            *n = sprintf( buf, "KL/0.1 406 Cannot Delete Device %s\n", str[1] );
        }
    }
    // SNIFF KL/version#
    else if ( strcmp(str[0], "SNIFF") == 0 )
    {
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

/***************************************************************************************
 * Everything related to electrical will reside here. 
 **************************************************************************************/

/* initialize sw variable, for RF transmission/receival */
void initialize_rc_switch()
{
    sw = new RCSwitch();
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

    /* disable pin after transmission */
    sw->disableTransmit();
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

    /* disable receive pin after data reception */
    sw->disableReceive();
}

/* initialize LEDs, for status indication */
void initialize_leds()
{
    LED0 = get_int( "electronics", "led_pin0", 21 );
    LED1 = get_int( "electronics", "led_pin1", 22 );
    LED2 = get_int( "electronics", "led_pin2", 23 );

    pinMode( LED0, OUTPUT );
    pinMode( LED1, OUTPUT );
    pinMode( LED2, OUTPUT );
}

/*
 * Accept basically any value to turn on LEDs. 
 * Though treating this as a boolean might be easier.
 */
void set_status_led( int led0, int led1, int led2 );
{
}

/***************************************************************************************
 * Everything related to configuration will reside here. 
 **************************************************************************************/

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

/***************************************************************************************
 * Everything related to database manipulation will reside here. 
 **************************************************************************************/

/* callback function for database access */
static int callback( void *data, int argc, char **argv, char **azColName )
{
    char lgbuf[2048];

    for ( int i = 0; i < argc; i++ )
    {
        if ( strcmp(azColName[i], "dev_name") == 0  )
        {
            if ( db_ptr->n < MAX_DEVICES )
            {
                db_ptr->name[db_ptr->n] = (char *) malloc( sizeof(char) * strlen(argv[i]) );
                strcpy( db_ptr->name[db_ptr->n], argv[i] );

                /* Increment number of entries used. */
                db_ptr->n++;
            }
        }
        else if ( strcmp(azColName[i], "dev_on") == 0 )
        {
            db_ptr->on = atoi( argv[i] );
        }
        else if ( strcmp(azColName[i], "dev_off") == 0 )
        {
            db_ptr->off = atoi( argv[i] );
        }
        else if ( strcmp(azColName[i], "dev_pulse") == 0 )
        {
            db_ptr->pulse = atoi( argv[i] );
        }
        else if ( strcmp(azColName[i], "dev_toggled") == 0 )
        {
            db_ptr->toggle = atoi( argv[i] );
        }
        else
        {
            sprintf( lgbuf, "Unknown column name %s", azColName[i] );
            write_to_log( lgbuf );
        }
    }

    return 0;
}

/*
 * Update toggle value when client requests toggle. 
 * Returns 1 when an error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 */
static int update_toggle( const char *name )
{
    sqlite3 *db;
    char *errmsg = 0;
    int status, rv;
    char sql[2048];
    char lgbuf[2048];

    /* Open database */
    status = sqlite3_open( get_string("database", "db_location", DBLOCATION), &db );

    if ( status )
    {
        sprintf( lgbuf, "Can't open database: %s", sqlite3_errmsg(db) );
        write_to_log( lgbuf );
        return -1;
    }

    /* Create the sql statement */
    sprintf( sql, "UPDATE device SET dev_toggled=%d WHERE dev_name='%s';", (! db_ptr->toggle), name );

    /* Execute sql statement */
    status = sqlite3_exec( db, sql, callback, 0, &errmsg );

    if ( status != SQLITE_OK )
    {
        sprintf( lgbuf, "sql error: %s", errmsg );
        write_to_log( lgbuf );
        sqlite3_free( errmsg );
        rv = 1;
    }
    else
    {
        sprintf( lgbuf, "successfully set device '%s' toggle from database", name );
        write_to_log( lgbuf );
        rv = 0;
    }

    sqlite3_close( db );

    return rv;
}

/* 
 * Insert new entry into database.
 * Returns 1 when an error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 */
static int add_device( const char *name, int on_code, int off_code, int pulse )
{
    sqlite3 *db;
    char *errmsg = 0;
    int status, rv;
    char sql[2048];
    char lgbuf[2048];

    /* Open database */
    status = sqlite3_open( get_string("database", "db_location", DBLOCATION), &db );

    if ( status )
    {
        sprintf( lgbuf, "Can't open database: %s", sqlite3_errmsg(db) );
        write_to_log( lgbuf );
        return -1;
    }

    /* Create the sql statement */
    sprintf( sql, "INSERT INTO device VALUES( '%s', %u, %u, %u, 0 );",
             name, on_code, off_code, pulse );
    
    /* Execute sql statement */
    status = sqlite3_exec( db, sql, callback, 0, &errmsg );

    if ( status != SQLITE_OK )
    {
        sprintf( lgbuf, "sql error: %s", errmsg );
        write_to_log( lgbuf );
        sqlite3_free( errmsg );
        rv = 1;
    }
    else
    {
        sprintf( lgbuf, "successfully added device '%s' to database", name );
        write_to_log( lgbuf );
        rv = 0;
    }

    sqlite3_close( db );

    return rv;
}

/* 
 * Delete entry from database.
 * Returns 1 when an error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 */
static int delete_device( const char *name )
{
    sqlite3 *db;
    char *errmsg = 0;
    int status, rv;
    char sql[2048];
    char lgbuf[2048];

    /* Open database */
    status = sqlite3_open( get_string("database", "db_location", DBLOCATION), &db );

    if ( status )
    {
        sprintf( lgbuf, "Can't open database: %s", sqlite3_errmsg(db) );
        write_to_log( lgbuf );
        return -1;
    }

    /* Create the sql statement */
    sprintf( sql, "DELETE FROM device WHERE dev_name='%s';", name );
    
    /* Execute sql statement */
    status = sqlite3_exec( db, sql, callback, 0, &errmsg );

    if ( status != SQLITE_OK )
    {
        sprintf( lgbuf, "sql error: %s", errmsg );
        write_to_log( lgbuf );
        sqlite3_free( errmsg );
        rv = 1;
    }
    else
    {
        sprintf( lgbuf, "successfully removed device '%s' from database", name );
        write_to_log( lgbuf );
        rv = 0;
    }

    sqlite3_close( db );

    return rv;
}

/* 
 * Retreive device information given the device name.
 * Returns 1 when an error occurs,
 * Returns -1 when unable to open db file
 * Returns 0 otherwise.
 */
static int select_device( const char *name )
{
    sqlite3 *db;
    char *errmsg = 0;
    int status, rv;
    char sql[2048];
    const char *data = "retreiving data\n";
    char lgbuf[2048];

    /* Open database */
    status = sqlite3_open( get_string("database", "db_location", DBLOCATION), &db );

    if ( status )
    {
        sprintf( lgbuf, "Can't open database: %s", sqlite3_errmsg(db) );
        write_to_log( lgbuf );
        return -1;
    }

    /* Create the sql statement */
    sprintf( sql, "SELECT * FROM device WHERE dev_name='%s';", name );

    /* Execute sql statement */
    status = sqlite3_exec( db, sql, callback, 0, &errmsg );

    if ( status != SQLITE_OK )
    {
        sprintf( lgbuf, "sql error: %s", errmsg );
        write_to_log( lgbuf );
        sqlite3_free( errmsg );
        rv = 1;
    }
    else
    {
        sprintf( lgbuf, "successfully retreived device '%s' data from database", name );
        write_to_log( lgbuf );
        rv = 0;
    }

    sqlite3_close( db );

    return rv;
}

/* 
 * Retreive list of devices stored.
 * Returns 1 when an error occurs,
 * Returns -1 when unable to open db file
 * Returns 0 otherwise.
 */
static int dump_devices()
{
    sqlite3 *db;
    char *errmsg = 0;
    int status, rv;
    char sql[2048];
    const char *data = "retreiving data\n";
    char lgbuf[2048];

    /* Open database */
    status = sqlite3_open( get_string("database", "db_location", DBLOCATION), &db );

    if ( status )
    {
        sprintf( lgbuf, "Can't open database: %s", sqlite3_errmsg(db) );
        write_to_log( lgbuf );
        return -1;
    }

    /* Create the sql statement */
    sprintf( sql, "SELECT dev_name FROM device;" );

    /* Execute sql statement */
    status = sqlite3_exec( db, sql, callback, 0, &errmsg );

    if ( status != SQLITE_OK )
    {
        sprintf( lgbuf, "sql error: %s", errmsg );
        write_to_log( lgbuf );
        sqlite3_free( errmsg );
        rv = 1;
    }
    else
    {
        sprintf( lgbuf, "successfully retreived device names from database" );
        write_to_log( lgbuf );
        rv = 0;
    }

    sqlite3_close( db );

    return rv;
}