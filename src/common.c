/*
 *  General and miscellaneous functions
 *  will reside here, for ease.
 *
 *  Written by: Christian Kissinger
 */

// system-related includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include <unistd.h>
#include <signal.h>

// local includes
#include "common.h"
#include "daemon.h"
#include "log.h"
#include "ini.h"

// pointer to config cfg;
static config *conf;

// pointers for database functions.
static char *sql_buf;
static db_data *data_out;

/* global variable for a db counter,
 * and another for entry size;
 */
static int db_counter = 0;
static int db_len = -1;

/* Some function declarations for future use */
static void reset_db_counter();
static int check_device_type( const int in );
static int get_digit_count( const int in );

/*******************************************************************************
 * Everything related to arg processing will reside here
 ******************************************************************************/
void process_args( int argc, char **argv )
{
    if ( argc >= 2 )
    {
        if (strncmp( argv[1], "daemon", 6) == 0 )
        {
            printf( "Not yet implemented\n" );
        }
    }
}


/*******************************************************************************
 * Everything related to configuration will reside here.
 ******************************************************************************/

/* a hanlder function for analyzing configurations */
static int handler(void* user, const char* section, const char* name,
                   const char* value)
{
    config *pconfig = (config *)user;

    /* a quick little shortcut that utilizes strncmp */
    #define MATCH(s, sl, n, nl) strncmp(section, s, sl) == 0 && \
    strncmp(name, n, nl) == 0

    // Network
    if ( MATCH("network", 8, "port", 4) )
    {
        pconfig->port = atoi( value );
    }
    else if ( MATCH("network", 8, "buffer_size", 12) )
    {
        pconfig->buffer_size = atoi( value );
    }
    // Mqtt
    else if ( MATCH("mqtt", 5, "mqtt_server", 11) )
    {
        pconfig->mqtt_server = strdup( value );
    }
    else if ( MATCH("mqtt", 5, "mqtt_port", 9) )
    {
        pconfig->mqtt_port = atoi( value );
    }
    else if ( MATCH("mqtt", 5, "recv_buff", 9) )
    {
        pconfig->recv_buff = atoi( value );
    }
    else if ( MATCH("mqtt", 5, "snd_buff", 9) )
    {
        pconfig->snd_buff = atoi( value );
    }
    else if ( MATCH( "mqtt", 5, "topic_buff", 11) )
    {
        pconfig->topic_buff = atoi( value );
    }
    else if ( MATCH( "mqtt", 5, "app_msg_buff", 13) )
    {
        pconfig->app_msg_buff = atoi( value );
    }
    // Database
    else if ( MATCH("database", 9, "db_location", 12) )
    {
        pconfig->db_loc = strdup( value );
    }
    else if ( MATCH("database", 9, "db_buff", 8) )
    {
        pconfig->db_buff = atoi( value );
    }
    else if ( MATCH("database", 9, "max_dev_count", 14) )
    {
        pconfig->max_dev_count = atoi( value );
    }
    // Default case
    else
    {
        return 0;  /* unknown section/name, error */
    }
    return 1;
}

/* Initialize configuration variable, to ini file */
int initialize_conf_parser( config *cfg )
{
    /* create an accessible pointer for the rest of the program if needed */
    conf = cfg;

    if ( ini_parse( CONFLOCATION, handler, cfg) < 0 )
    {
        fprintf( stderr, "Cannot load config file '%s'\n", CONFLOCATION );
        return 1;
    }

    return 0;
}

/*******************************************************************************
 * Everything related to database manipulation will reside here.
 ******************************************************************************/

/*
 * Initialize everything for database usage.
 * Returns 0 for a success, 1 for any error that may occur.
 */
void initialize_db( char *sql_buffer, db_data *dat )
{
    /* establish a local pointer for sql_buffer */
    sql_buf = sql_buffer;
    data_out = dat;
}

/* Function to verify device type, or
 * return -1 if invalid.
 */
static int check_device_type( const int in )
{
    int rv = -1;

    switch( in )
    {
        case 0:
            rv = 0;
            break;
        default:
            rv = -1;
            break;
    }

    return rv;
}

/*
 * get the digit count for a particular
 * integer, returns the length.
 */
static int get_digit_count( const int in )
{
    int count = 0;

    if ( in == 0 )
    {
        count = 1;
    }
    else
    {
        int temp = in;

        while ( temp != 0 )
        {
            temp /= 10;
            count++;
        }
    }

    return count;
}

/* callback function for database access */
static int db_callback( void *data, int argc, char **argv, char **azColName )
{
    for ( int i = 0; i < argc; i++ )
    {
        if ( strncmp(azColName[i], "dev_name", 8) == 0  )
        {
            strncpy( data_out[db_counter].dev_name,
            argv[i], (sizeof(argv[i])/sizeof(char)) );
        }
        else if ( strncmp(azColName[i], "mqtt_topic", 10) == 0 )
        {
            strncpy( data_out[db_counter].mqtt_topic,
            argv[i], (sizeof(argv[i])/sizeof(char)) );
        }
        else if ( strncmp(azColName[i], "dev_type", 8) == 0 )
        {
            data_out[db_counter].dev_type = atoi( argv[i] );
        }
        // a special case to get the count
        else if ( strncmp( azColName[i], "COUNT(*)", 8) == 0 )
        {
            /* set the global variable */
            db_len = atoi( argv[i] );
        }
        else
        {
            log_error( "Unknown column name %s", azColName[i] );
        }
    }

    db_counter++;

    return 0;
}

/*
 * Read from Database file.
 * Returns 1 when an SQL error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 */
static int execute_db_query( char *query )
{
    sqlite3 *db;
    char *errmsg = 0;
    int status, rv;

    /* Open database */
    status = sqlite3_open( conf->db_loc, &db );

    if ( status )
    {
        log_error( "Can't open database: %s", sqlite3_errmsg(db) );
        sqlite3_close( db );
        return -1;
    }

    /* Execute sql statement */
    status = sqlite3_exec( db, query, db_callback, 0, &errmsg );

    if ( status != SQLITE_OK )
    {
        log_warn( "sql error: %s", errmsg );
        sqlite3_free( errmsg );
        rv = 1;
    }
    else
    {
        // update this at some point
        log_debug( "Succesfully ran query to database" );
        rv = 0;
    }

    sqlite3_close( db );

    /* Reset db counter */
    reset_db_counter();

    return rv;
}

/*
 * A simple way to reset the global counter after an operation is complete
 */
static void reset_db_counter()
{
    db_counter = 0;
}

/*
 * a way for other functions to get the entry length ahead of time.
 * And this simplifies it for the programmer a bit.
 *
 * Note from Christian: I am aware that this does break consistency
 * with other functions, but since a length of 1 is possible,
 * that is my reasoning for returning either a 0 or -1 for any errors.
 *
 * Returns 0 when an SQL error occurs,
 * Returns -1 when unable to open db file,
 * Returns actual length otherwise.
 */
int get_db_len()
{
    strncpy( sql_buf, "SELECT COUNT(*) FROM device;", 29);

    int db_ret = execute_db_query( sql_buf );

    if ( db_ret < 0 )
    {
        log_error( "Error opening sqlite3 file" );
    }
    else if ( db_ret )
    {
        log_error( "Some sort of SQL error occurred" );
        db_ret = 0;
    }

    log_trace( "Amount of devices currently: %d", db_len );

    /* memset the sql buffer */
    memset( sql_buf, 0, conf->db_buff );

    return db_len;

}

/**
 * @brief The insert function, it inserts a new entry.
 *
 * Currently:
 * @param dev_name is for the device name
 * @param mqtt_topic is for the mqtt topic (may or may not differ from dev_name)
 * @param type is for device type, for now only 0 is accepted.
 *
 * @note Returns 2 when a user error occurs,
 * Returns 1 when an SQL error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 */
int insert_db_entry( char *dev_name, char *mqtt_topic, const int type )
{
    /* prepare insert statement */
    /* determine type digit count */
    int verified_type = check_device_type( type );

    /* Make sure it is valid first */
    if ( verified_type == -1 )
    {
        /* Forget about this, won't even process this query. */
        log_error( "invalid device type %d", type );

        /* memset the sql buffer */
        memset( sql_buf, 0, conf->db_buff );

        return 2;
    }

    int count = get_digit_count( verified_type );

    /* put it all together */
    int n = snprintf( sql_buf, (39 + strlen(dev_name) +
    strlen(mqtt_topic) + count),
    "INSERT INTO device VALUES( '%s', '%s', %d );",
    dev_name, mqtt_topic, type );

    /* Actually execute the insert db query */
    int db_ret = execute_db_query( sql_buf );

    if ( db_ret < 0 )
    {
        log_error( "Error opening sqlite3 file" );
    }
    else if ( db_ret )
    {
        log_error( "Some sort of SQL error occurred" );
    }

    /* memset the sql buffer */
    memset( sql_buf, 0, conf->db_buff );

    /* Return the result for the server */
    return db_ret;
}

/*
 * The select query, does the work
 * of actually getting the desired
 * entry.
 *
 * Returns 2 when a user error occurs,
 * Returns 1 when an SQL error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 */
int select_db_entry( char *dev_name, char *mqtt_topic )
{
    int n = -1;

    // dev_name is our criteria
    if ( mqtt_topic == NULL )
    {
        n = snprintf( sql_buf, (69 + strlen(dev_name)),
        "SELECT dev_name, mqtt_topic, dev_type FROM device" \
        " WHERE dev_name='%s';", dev_name );
    }
    // mqtt_topic is our criteria
    else if ( dev_name == NULL)
    {
        n = snprintf( sql_buf, (71 + strlen(mqtt_topic)),
        "SELECT dev_name, mqtt_topic, dev_type FROM device" \
        " WHERE mqtt_topic='%s';", mqtt_topic );
    }
    // both are used
    else if ( dev_name != NULL && mqtt_topic != NULL )
    {
        n = snprintf( sql_buf, (87 + strlen(dev_name) + strlen(mqtt_topic)),
        "SELECT dev_name, mqtt_topic, dev_type FROM device" \
        " WHERE dev_name='%s' AND mqtt_topic='%s';", dev_name, mqtt_topic );
    }
    // should NEVER EVER be accessed
    else
    {
        /* Forget about this, won't even process this query. */
        log_error( "come now both mqtt_topic and" \
        " dev_name can't be null!" );

        /* memset the sql buffer */
        memset( sql_buf, 0, conf->db_buff );

        return 2;
    }


    int db_ret = execute_db_query( sql_buf );

    if ( db_ret < 0 )
    {
        log_error( "Error opening sqlite3 file" );
    }
    else if ( db_ret )
    {
        log_error( "Some sort of SQL error occurred" );
    }
    else
    {
        log_trace( "entry retreived: %s - %s - %d",
        data_out[0].dev_name, data_out[0].mqtt_topic, data_out[0].dev_type );
    }

    /* memset the sql buffer */
    memset( sql_buf, 0, conf->db_buff );

    return db_ret;
}

/*
 * A specialized select function
 * which has the primary function of dumping
 * existing entries.
 *
 * Returns 1 when an SQL error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 */
int dump_db_entries()
{
    strncpy( sql_buf, "SELECT dev_name, mqtt_topic, dev_type FROM device;",
             51 );

    int db_ret = execute_db_query( sql_buf );

    if ( db_ret < 0 )
    {
        log_error( "Error opening sqlite3 file" );
    }
    else if ( db_ret )
    {
        log_error( "Some sort of SQL error occurred" );
    }
    else
    {
        log_trace( "%d entries retreived", db_len );
    }

    /* memset the sql buffer */
    memset( sql_buf, 0, conf->db_buff );

    return db_ret;
}

/*
 * This is the function to update entries.
 *
 * As far as I (Christian) can tell, it looks
 * like the preliminary checks for ambiguous
 * criteria, like odev_name is NULL but
 * ndev_name for some reason is not.
 *
 * Returns 2 when a user error occurs,
 * Returns 1 when an SQL error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 */
int update_db_entry( char *odev_name, char *ndev_name,
                     char *omqtt_topic, char *nmqtt_topic,
                     const int ndev_type )
{
    /*
     * Without a doubt there is likely a better
     * approach than what is done here, but
     * it made sense at the time, and handles my
     * worst cases.
     */

    /* Run preliminary checks before executing */
    const int verified_ndev_type = check_device_type( ndev_type );

    if ( odev_name == NULL && ndev_name != NULL )
    {
        log_error( "Ambiguity regarding changing dev_name" );

        /* memset the sql buffer */
        memset( sql_buf, 0, conf->db_buff );

        return 2;
    }
    else if ( omqtt_topic == NULL && nmqtt_topic != NULL )
    {
        log_error( "Ambiguity regarding changing mqtt_topic" );

        /* memset the sql buffer */
        memset( sql_buf, 0, conf->db_buff );

        return 2;
    }
    else if ( ( (odev_name == NULL)
             && (omqtt_topic == NULL) )
           && verified_ndev_type == -1 )
    {
        log_error( "Ambiguity regarding changing dev_type" );

        /* memset the sql buffer */
        memset( sql_buf, 0, conf->db_buff );

        return 2;
    }

    int n = -1;

    // Update just dev_name
    if ( (omqtt_topic == NULL && nmqtt_topic == NULL)
      && (verified_ndev_type == -1) )
    {
        n = snprintf( sql_buf, (49 + strlen(odev_name) + strlen(ndev_name)),
        "UPDATE device SET dev_name='%s' WHERE dev_name='%s';",
        ndev_name, odev_name );
    }
    // Update just mqtt_topic
    else if ( (odev_name == NULL && ndev_name == NULL)
           && (verified_ndev_type == -1) )
    {
        n = snprintf( sql_buf, (53 + strlen(omqtt_topic) +
        strlen(nmqtt_topic)),
        "UPDATE device SET mqtt_topic='%s' WHERE mqtt_topic='%s';",
        nmqtt_topic, omqtt_topic );
    }
    // Update just dev_type (odev_name NOT null)
    else if ( (odev_name != NULL)
           && (omqtt_topic == NULL)
           && (verified_ndev_type != -1) )
    {
        n = snprintf( sql_buf, (47 + strlen(odev_name) +
        get_digit_count(ndev_type)),
        "UPDATE device SET dev_type=%d WHERE dev_name='%s';",
        ndev_type, odev_name );
    }
    // Update just dev_type (omqtt_topic NOT null) **(CAN AFFECT MULTIPLE)
    else if ( (odev_name == NULL)
           && (omqtt_topic != NULL)
           && (verified_ndev_type != -1) )
    {
        n = snprintf( sql_buf, (49 + strlen(omqtt_topic) +
        get_digit_count(ndev_type)),
        "UPDATE device SET dev_type=%d WHERE mqtt_topic='%s';",
        ndev_type, omqtt_topic );
    }
    // Update just dev_type (odev_name AND omqtt_topic NOT null)
    else if ( (odev_name != NULL)
           && (omqtt_topic != NULL)
           && (verified_ndev_type != -1) )
    {
        n = snprintf( sql_buf, (65 + strlen(odev_name) +
        strlen(omqtt_topic) + get_digit_count(ndev_type)),
        "UPDATE device SET dev_type=%d WHERE dev_name='%s'" \
        " AND mqtt_topic='%s';",
         ndev_type, odev_name, omqtt_topic );
    }
    // Update dev_name AND mqtt_topic
    else if ( (odev_name != NULL && ndev_name != NULL)
           && (omqtt_topic != NULL && nmqtt_topic != NULL)
           && (verified_ndev_type == -1) )
    {
        n = snprintf( sql_buf, (82 + strlen(odev_name) +
        strlen(ndev_name) + strlen(omqtt_topic) + strlen(nmqtt_topic)),
        "UPDATE device SET dev_name='%s', mqtt_topic='%s' WHERE" \
        " dev_name='%s' AND mqtt_topic='%s';",
         ndev_name, nmqtt_topic, odev_name, omqtt_topic);
    }
    // Update mqtt_topic AND dev_type
    else if ( (odev_name == NULL && ndev_name == NULL)
           && (omqtt_topic != NULL && nmqtt_topic != NULL)
           && (verified_ndev_type != -1) )
    {
        n = snprintf( sql_buf, (64 + strlen(omqtt_topic) +
        strlen(nmqtt_topic) + get_digit_count(ndev_type)),
        "UPDATE device SET mqtt_topic='%s', dev_type=%d WHERE" \
        " mqtt_topic='%s';",
        nmqtt_topic, ndev_type, omqtt_topic );
    }
    // Update dev_name AND dev_type
    else if ( (odev_name != NULL && ndev_name != NULL)
           && (omqtt_topic == NULL && nmqtt_topic == NULL)
           && (verified_ndev_type != -1) )
    {
        n = snprintf( sql_buf, (60 + strlen(odev_name) + strlen(ndev_name) +
        get_digit_count(ndev_type)),
        "UPDATE device SET dev_name='%s', dev_type=%d WHERE" \
        " dev_name='%s';",
         ndev_name, ndev_type, odev_name);
    }
    // Update dev_name, mqtt_topic, AND dev_type
    else if ( (odev_name != NULL && ndev_name != NULL)
           && (omqtt_topic != NULL && nmqtt_topic != NULL)
           && (verified_ndev_type != -1) )
    {
        n = snprintf( sql_buf, (93 + strlen(odev_name) + strlen(ndev_name) +
        strlen(omqtt_topic) + strlen(nmqtt_topic) +
        get_digit_count(ndev_type)),
        "UPDATE device SET dev_name='%s', mqtt_topic='%s', dev_type=%d" \
        " WHERE dev_name='%s' AND mqtt_topic='%s';",
         ndev_name, nmqtt_topic, ndev_type, odev_name, omqtt_topic );
    }
    // default case, and shouldn't ever be reached.
    else
    {
        /* Forget about this, won't even process this query. */
        log_error( "come now mqtt_topic and dev_name" \
        " can't be null, and ndev_type can't be -1 all" \
        " at the same time!" );

        /* memset the sql buffer */
        memset( sql_buf, 0, conf->db_buff );

        return 2;
    }

    int db_ret = execute_db_query( sql_buf );

    if ( db_ret < 0 )
    {
        log_error( "Error opening sqlite3 file" );
    }
    else if ( db_ret )
    {
        log_error( "Some sort of SQL error occurred" );
    }
    else
    {
        log_trace( "entry updated successfully" );
    }

    /* memset the sql buffer */
    memset( sql_buf, 0, conf->db_buff );

    return db_ret;
}

/*
 * This is the way to remove entries.
 *
 * Beware that if mqtt_topic is passed in
 * it can potentially remove multiple
 * entries.
 *
 * Returns 1 when an SQL error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 */
int delete_db_entry( char *dev_name, char *mqtt_topic )
{
    int n = -1;

    // Delete entry that matches dev_name
    if ( mqtt_topic == NULL )
    {
        n = snprintf( sql_buf, (38 + strlen(dev_name)),
        "DELETE FROM device WHERE dev_name='%s';",
        dev_name );
    }
    // Delete entry's that match mqtt_topic (allows multiple to be deleted)
    else if ( dev_name == NULL )
    {
        n = snprintf( sql_buf, (40 + strlen(mqtt_topic)),
        "DELETE FROM device WHERE mqtt_topic='%s';",
        mqtt_topic );
    }
    // Delete entry that meets this criteria
    else if ( dev_name != NULL && mqtt_topic != NULL )
    {
        n = snprintf( sql_buf, (56 + strlen(dev_name) + strlen(mqtt_topic)),
        "DELETE FROM device WHERE dev_name='%s' AND mqtt_topic='%s';",
        dev_name, mqtt_topic );
    }
    // Should never EVER be reached
    else
    {
        /* Forget about this, won't even process this query. */
        log_error( "come now both mqtt_topic and" \
        " dev_name can't be null!" );

        /* memset the sql buffer */
        memset( sql_buf, 0, conf->db_buff );

        return 2;
    }

    int db_ret = execute_db_query( sql_buf );

    if ( db_ret < 0 )
    {
        log_error( "Error opening sqlite3 file" );
    }
    else if ( db_ret )
    {
        log_error( "Some sort of SQL error occurred" );
    }

    // Maybe this should be verified? I'll keep this here just in case.
    log_trace( "entry removed: %s - %s", dev_name, mqtt_topic );

    /* memset the sql buffer */
    memset( sql_buf, 0, conf->db_buff );

    return db_ret;
}

/* Has the soul purpose of clearing db_data buffer for reuse */
void reset_db_data()
{
    int db_ret = get_db_len();

    if ( db_ret < 0 )
    {
        log_error( "Error opening sqlite3 file" );
    }
    else if ( db_ret )
    {
        log_warn( "Unable to update db_len, using as is" );
    }

    /* Actually reset the db_data struct*/
    for (int i = db_len; i < db_len; i++ )
    {
        memset( data_out[i].dev_name, 0, DB_DATA_LEN );
        memset( data_out[i].mqtt_topic, 0, DB_DATA_LEN );
        data_out[i].dev_type = -1;
    }
}
