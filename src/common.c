/*
 * General and miscellaneous functions
 * will reside here, for ease.
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
static db_data *memory;
static char *dev_type_str;

/*
 * global variable for a db counter,
 * and another for entry size;
 */
static int db_counter = 0;
static int db_len = -1;
static int *to_change;

// System pointers
static pthread_mutex_t *lock;
static sem_t *mutex;

/* Some function prototypes for future use */
static void reset_db_counter();
static int get_db_len();
static int insert_db_entry( char *dev_name, char *mqtt_topic, const int type );
static int dump_db_entries();
static int update_db_entry( char *odev_name, char *ndev_name,
                     char *omqtt_topic, char *nmqtt_topic,
                     const int ndev_type );
static int update_db_dev_state( char *dev_name, char *mqtt_topic,
                         const int new_state );
static int delete_db_entry( char *dev_name, char *mqtt_topic );

/*******************************************************************************
 * Miscellaneous useful functions will be posted here.
 ******************************************************************************/

/**
 * @brief Function to verify device type.
 *
 * @param in the unknown device ID to be passed in.
 *
 * @note return -1 if invalid,
 * proper device id otherwise.
 */
int check_device_type( const int in )
{
    int rv = -1;

    switch( in )
    {
        case 0:
            rv = 0;
            break;

        case 1:
            rv = 1;
            break;

        case 2:
            rv = 2;
            break;

        case 3:
            rv = 3;
            break;

        default:
            rv = -1;
            break;
    }

    return rv;
}

/**
 * @brief Function to convert dev_type to string.
 *
 * @param in the Device ID to be passed in.
 *
 * @note returns the string created.
 */
char *device_type_to_str( const int in )
{
    switch( in )
    {
        case 0:
            strncpy( dev_type_str, "outlet/toggleable", 18 );
            break;

        case 1:
            strncpy( dev_type_str, "powerstrip", 11 );
            break;

        case 2:
            strncpy( dev_type_str, "rgbbulb", 8 );
            break;

        case 3:
            strncpy( dev_type_str, "dimmablebulb", 13 );
            break;

        default:
            strncpy( dev_type_str, "unknown/other", 14 );
            break;
    }

    return dev_type_str;
}

/**
 * @brief get the digit count for a particular integer.
 *
 * @param in the integer of interest.
 *
 * @note returns digit count.
 */
int get_digit_count( const int in )
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

/*******************************************************************************
 * Everything related to arg processing will reside here
 ******************************************************************************/

/**
 * TODO: This should be more fleshed out,
 * maybe a student can help with this part.
 *
 * @brief Process arguments passed in (from main).
 *
 * @param argc passing in argc from main would be easiest.
 * @param argv passing in argv from main would be easiest.
 *
 * @note returns 0 for success, 1 for error
 */
int process_args( int argc, char **argv )
{
    int rv = 0;

    if ( argc >= 2 )
    {
        if ( strncmp( argv[1], "daemon", 7) == 0 )
        {
            rv = run_as_daemon();
        }
    }

    return rv;
}


/*******************************************************************************
 * Everything related to configuration will reside here.
 ******************************************************************************/

/**
 * @brief a callback hanlder function for analyzing configuration settings.
 *
 * @param user the struct for configuration usage.
 * @param section the section of a ini file.
 * @param name the name of the section file.
 * @param value the string/value of interest.
 *
 * @note refer to the ini.c and ini.h for more information
 * and implementation on how it works
 */
static int ini_callback_handler( void* user, const char* section,
                                 const char* name, const char* value )
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

/**
 * @brief Initialize configuration variable, to ini file.
 *
 * @param cfg the config struct where configuration will be stored.
 *
 * @note check the return value and handle accordingly.
 *
 * returns 1 for any error that may occur,
 * returns 0 otherwise.
 */
int initialize_conf_parser( config *cfg )
{
    /* create an accessible pointer for the rest of the program if needed */
    conf = cfg;

    if ( ini_parse( CONFLOCATION, ini_callback_handler, cfg) < 0 )
    {
        fprintf( stderr, "Cannot load config file '%s'\n", CONFLOCATION );
        return 1;
    }

    return 0;
}

/*******************************************************************************
 * Everything related to database manipulation will reside here.
 ******************************************************************************/

/**
 * @brief Initialize everything for database thread usage.
 *
 * @param sql_buffer the buffer meant for sql statements.
 * @param dat the struct array for items in database.
 * @param to_chng the int array to indicate what may or may need to be changed.
 * @param lck the pthread mutex to prevent any race conditions.
 * @param mtx the semaphore mutex to also prevent any race conditions.
 *
 * @note
 * Returns 1 when an SQL error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 */
int initialize_db( char *sql_buffer, db_data *dat, int *to_chng, char *dv_str,
                   pthread_mutex_t *lck, sem_t *mtx )
{
    /*
     * establish a local pointer for sql_buffer,
     * and copy everything in database to memory.
     */
    sql_buf = sql_buffer;
    memory = dat;
    to_change = to_chng;
    dev_type_str = dv_str;

    /* establish a local pointer for lock semaphore */
    lock = lck;
    mutex = mtx;

    /* Get the count */
    int status = get_db_len();

    if ( status < -1 )
    {
        log_error( "Error opening sqlite3 file" );
        return -1;
    }
    else if ( status < 0 )
    {
        log_error( "Some sort of SQL error occurred" );
        return 1;
    }

    /* Migrate everything to memory */
    status = dump_db_entries();

    if ( status < 0 )
    {
        log_error( "Error opening sqlite3 file" );
        return -1;
    }
    else if ( status )
    {
        log_error( "Some sort of SQL error occurred" );
        return 1;
    }

    return 0;

}

/**
 * @brief Function to get current entry count
 *
 * @note this differs from get_db_len(), this function
 * just returns the count at that particular period in time.
 */
const int get_current_entry_count()
{
    return db_len;
}

/**
 * @brief callback function for database access.
 *
 * @param data Not actually used.
 * @param argc the amount of columns.
 * @param argv the content in the column.
 * @param azColName the column names.
 *
 * @note refer to sqlite3 documentation for more information.
 */
static int db_callback( void *data, int argc, char **argv, char **azColName )
{
    for ( int i = 0; i < argc; i++ )
    {
        if ( strncmp(azColName[i], "dev_name", 9) == 0  )
        {
            strncpy( memory[db_counter].dev_name,
            argv[i], strlen(argv[i]) );
        }
        else if ( strncmp(azColName[i], "mqtt_topic", 11) == 0 )
        {
            strncpy( memory[db_counter].mqtt_topic,
            argv[i], strlen(argv[i]) );

            /* Also copy full topic as well */
            snprintf( memory[db_counter].stat_topic, (12 + strlen(argv[i])),
                      "stat/%s/POWER", argv[i] );
            snprintf( memory[db_counter].cmnd_topic, (12 + strlen(argv[i])),
                      "cmnd/%s/POWER", argv[i] );
        }
        else if ( strncmp(azColName[i], "dev_type", 9) == 0 )
        {
            memory[db_counter].dev_type = atoi( argv[i] );
        }
        else if ( strncmp(azColName[i], "dev_state", 10) == 0 )
        {
            memory[db_counter].dev_state = atoi( argv[i] );
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

/**
 * @brief Handles the actual database query execution.
 *
 * @param query The desired SQL query as a char buffer.
 *
 * @note
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

/**
 * @brief A simple way to reset the global counter after an operation
 * is complete.
 */
static void reset_db_counter()
{
    db_counter = 0;
}

/**
 * @brief a way for other functions to get the entry length ahead of time.
 * And this simplifies it for the programmer a bit.
 *
 * @note note from Christian: I am aware that this does break consistency
 * with other functions, but since a length of 0 is possible,
 * that is my reasoning for returning either a -1 or -2 for any errors.
 *
 * Returns -1 when an SQL error occurs,
 * Returns -2 when unable to open db file,
 * Returns actual length otherwise.
 */
static int get_db_len()
{
    strncpy( sql_buf, "SELECT COUNT(*) FROM device;", 29);

    int db_ret = execute_db_query( sql_buf );

    if ( db_ret < 0 )
    {
        log_error( "Error opening sqlite3 file" );
        db_ret = -2;
    }
    else if ( db_ret )
    {
        log_error( "Some sort of SQL error occurred" );
        db_ret = -1;
    }

    /* only if everything is successful */
    if ( db_len >= 0 )
    {
        log_trace( "Amount of devices currently: %d", db_len );
        db_ret = db_len;
    }

    /* memset the sql buffer */
    memset( sql_buf, 0, conf->db_buff );

    return db_ret;
}

/**
 * @brief The insert function, it inserts a new entry.
 *
 * Currently:
 * @param dev_name is for the device name.
 * @param mqtt_topic is for the mqtt topic
 * (may or may not differ from dev_name)
 * @param type is for device type.
 *
 * @note refer to check_device_type() for valid device type.
 *
 * Returns 1 when an SQL error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 */
static int insert_db_entry( char *dev_name, char *mqtt_topic, const int type )
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
    /* dev_state is currently unknown so -1 */
    snprintf( sql_buf, (43 + strlen(dev_name) +
    strlen(mqtt_topic) + count),
    "INSERT INTO device VALUES( '%s', '%s', %d, -1 );",
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

/**
 * @brief A specialized select function
 * which has the primary function of dumping
 * existing entries.
 *
 * @note Returns 1 when an SQL error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 */
static int dump_db_entries()
{
    strncpy( sql_buf, "SELECT dev_name, mqtt_topic, dev_type," \
            " dev_state FROM device;",
             62 );

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

/**
 * @brief This is the function to update entries.
 *
 * @param odev_name old dev_name.
 * @param ndev_name new dev_name.
 * @param omqtt_topic old mqtt_topic.
 * @param nmqtt_topic new mqtt_topic.
 * @param ndev_type new dev_type.
 *
 * @note As far as I (Christian) can tell, it looks
 * like the preliminary checks for ambiguous
 * criteria, like odev_name is NULL but
 * ndev_name for some reason is not.
 *
 * Returns 1 when an SQL error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 */
static int update_db_entry( char *odev_name, char *ndev_name,
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

    // Update just dev_name
    if ( (omqtt_topic == NULL && nmqtt_topic == NULL)
      && (verified_ndev_type == -1) )
    {
        snprintf( sql_buf, (49 + strlen(odev_name) + strlen(ndev_name)),
        "UPDATE device SET dev_name='%s' WHERE dev_name='%s';",
        ndev_name, odev_name );
    }
    // Update just mqtt_topic
    else if ( (odev_name == NULL && ndev_name == NULL)
           && (verified_ndev_type == -1) )
    {
        snprintf( sql_buf, (53 + strlen(omqtt_topic) +
        strlen(nmqtt_topic)),
        "UPDATE device SET mqtt_topic='%s' WHERE mqtt_topic='%s';",
        nmqtt_topic, omqtt_topic );
    }
    // Update just dev_type (odev_name NOT null)
    else if ( (odev_name != NULL)
           && (omqtt_topic == NULL)
           && (verified_ndev_type != -1) )
    {
        snprintf( sql_buf, (47 + strlen(odev_name) +
        get_digit_count(ndev_type)),
        "UPDATE device SET dev_type=%d WHERE dev_name='%s';",
        ndev_type, odev_name );
    }
    // Update just dev_type (omqtt_topic NOT null) **(CAN AFFECT MULTIPLE)
    else if ( (odev_name == NULL)
           && (omqtt_topic != NULL)
           && (verified_ndev_type != -1) )
    {
        snprintf( sql_buf, (49 + strlen(omqtt_topic) +
        get_digit_count(ndev_type)),
        "UPDATE device SET dev_type=%d WHERE mqtt_topic='%s';",
        ndev_type, omqtt_topic );
    }
    // Update just dev_type (odev_name AND omqtt_topic NOT null)
    else if ( (odev_name != NULL)
           && (omqtt_topic != NULL)
           && (verified_ndev_type != -1) )
    {
        snprintf( sql_buf, (65 + strlen(odev_name) +
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
        snprintf( sql_buf, (82 + strlen(odev_name) +
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
        snprintf( sql_buf, (64 + strlen(omqtt_topic) +
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
        snprintf( sql_buf, (60 + strlen(odev_name) + strlen(ndev_name) +
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
        snprintf( sql_buf, (93 + strlen(odev_name) + strlen(ndev_name) +
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


/**
 * @brief This has the job of updating a dev_state
 * given dev_name, mqtt_topic, or both.
 *
 * @param dev_name the device name of the device that changed states.
 * @param mqtt_topic the mqtt topic of the device that changed states.
 * @param new_state the new state value.
 *
 * @note Returns 1 when an SQL error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 *
 * @todo a quirk here could be the state can be set to some unkown state,
 * but it could be useful for if/when RGB bulbs get implemented..
 */
static int update_db_dev_state( char *dev_name, char *mqtt_topic,
                         const int new_state )
{
    // dev_name is our criteria
    if ( mqtt_topic == NULL )
    {
        snprintf( sql_buf, (48 + strlen(dev_name) +
        get_digit_count(new_state)),
        "UPDATE device SET dev_state=%d WHERE dev_name='%s';",
        new_state, dev_name );
    }
    // mqtt_topic is our criteria
    else if ( dev_name == NULL)
    {
        snprintf( sql_buf, (50 + strlen(mqtt_topic) +
        get_digit_count(new_state)),
        "UPDATE device SET dev_state=%d WHERE mqtt_topic='%s';",
        new_state, mqtt_topic );
    }
    // both are used
    else if ( dev_name != NULL && mqtt_topic != NULL )
    {
        snprintf( sql_buf, (66 + strlen(dev_name) + strlen(mqtt_topic) +
        get_digit_count(new_state)),
        "UPDATE device SET dev_state=%d WHERE dev_name='%s' AND" \
        " mqtt_topic='%s';",
        new_state, dev_name, mqtt_topic );
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
        log_trace( "entry updated successfully" );
    }

    /* memset the sql buffer */
    memset( sql_buf, 0, conf->db_buff );

    return db_ret;
}

/**
 * @brief This is the way to remove entries.
 *
 * @param dev_name the device name to be removed.
 * @param mqtt_topic the mqtt_topic of device to be removed.
 *
 * @note Beware that if mqtt_topic is passed in without dev_name also
 * passed in can potentially remove multiple entries.
 *
 * Returns 1 when an SQL error occurs,
 * Returns -1 when unable to open db file,
 * Returns 0 otherwise.
 */
static int delete_db_entry( char *dev_name, char *mqtt_topic )
{
    // Delete entry that matches dev_name
    if ( mqtt_topic == NULL )
    {
        snprintf( sql_buf, (38 + strlen(dev_name)),
        "DELETE FROM device WHERE dev_name='%s';",
        dev_name );
    }
    // Delete entry's that match mqtt_topic (allows multiple to be deleted)
    else if ( dev_name == NULL )
    {
        snprintf( sql_buf, (40 + strlen(mqtt_topic)),
        "DELETE FROM device WHERE mqtt_topic='%s';",
        mqtt_topic );
    }
    // Delete entry that meets this criteria
    else if ( dev_name != NULL && mqtt_topic != NULL )
    {
        snprintf( sql_buf, (56 + strlen(dev_name) + strlen(mqtt_topic)),
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

/**
 * @brief the data refresher which updates database
 * at every roughly 5 seconds, if there is
 * a change to be made of course.
 *
 * @param args is not used atm.
 */
void *db_updater( void* args )
{
    /* sleep for 5 seconds initially */
    usleep(100000*50U);

    while( 1 )
    {
        sem_wait( mutex );
        pthread_mutex_lock( lock );

        /*
         * Critical Section
         */
        for ( int i = 0; i < conf->max_dev_count; i++ )
        {
            switch( to_change[i] )
            {
                // No changes needed
                case -1:
                    /* Do Nothing */
                    break;

                // Update i's dev_state
                case 0:
                {
                    update_db_dev_state( memory[i].dev_name,
                                         memory[i].mqtt_topic,
                                         memory[i].dev_state );

                    /* nothing to reset, so continue */
                    break;
                }

                // Update i's dev_name
                case 1:
                {
                    update_db_entry( memory[i].odev_name, memory[i].dev_name,
                                     NULL, NULL, -1 );

                    /* reset for later use */
                    memset( memory[i].odev_name, 0, DB_DATA_LEN );

                    break;
                }

                // Update i's mqtt_topic
                case 2:
                {
                    update_db_entry( NULL, NULL, memory[i].omqtt_topic,
                                     memory[i].mqtt_topic, -1 );

                    /* reset for later use */
                    memset( memory[i].omqtt_topic, 0, DB_DATA_LEN );

                    break;
                }

                // Update i's device_type
                case 3:
                {
                    update_db_entry( memory[i].dev_name, NULL,
                                     memory[i].mqtt_topic, NULL,
                                     memory[i].dev_type );

                    /* There is nothing to reset, this is done */
                    break;
                }

                // Update all (makes it easier)
                case 4:
                {
                    /* Update dev_state first just to be safe */
                    update_db_dev_state( memory[i].dev_name,
                                         memory[i].mqtt_topic,
                                         memory[i].dev_state );

                    /* Now to update the rest */
                    update_db_entry( memory[i].odev_name, memory[i].dev_name,
                                     memory[i].omqtt_topic,
                                     memory[i].mqtt_topic,
                                     memory[i].dev_type );

                    /* reset for later use */
                    memset( memory[i].odev_name, 0, DB_DATA_LEN );
                    memset( memory[i].omqtt_topic, 0, DB_DATA_LEN );

                    break;
                }

                // Add new device that's in i
                case 5:
                {
                    insert_db_entry( memory[i].dev_name, memory[i].mqtt_topic,
                                     memory[i].dev_type );

                    get_db_len();

                    break;
                }

                // remove i's device
                case 6:
                {
                    /*
                     * this time we do want to verify that
                     * the query ran correctly, and delete
                     * things only afterwards.
                     */
                    int db_ret = -1;

                    if ( memory[i].odev_name[0] != '\0'
                      && memory[i].omqtt_topic[0] != '\0' )
                    {
                        db_ret = delete_db_entry( memory[i].odev_name,
                                                  memory[i].omqtt_topic );
                    }
                    else if ( memory[i].odev_name[0] != '\0'
                           && memory[i].omqtt_topic[0] == '\0' )
                    {
                        db_ret = delete_db_entry( memory[i].odev_name,
                                                  memory[i].mqtt_topic );
                    }
                    else if ( memory[i].odev_name[0] == '\0'
                           && memory[i].omqtt_topic[0] != '\0' )
                    {
                        db_ret = delete_db_entry( memory[i].dev_name,
                                                  memory[i].omqtt_topic );
                    }
                    else
                    {
                        db_ret = delete_db_entry( memory[i].dev_name,
                                                  memory[i].mqtt_topic );
                    }


                    if ( db_ret < 0 )
                    {
                        log_error( "Error opening sqlite3 file, "\
                                   "not removing entry" );
                    }
                    else if ( db_ret )
                    {
                        log_error( "desired entry not deleted due to SQL" \
                                   " Error" );

                    }
                    else
                    {
                        /* Update the db_len variable */
                        get_db_len();

                        /* reset for later use */
                        memset( memory[i].dev_name, 0, DB_DATA_LEN );
                        memset( memory[i].mqtt_topic, 0, DB_DATA_LEN );
                        memory[i].dev_type = -1;
                        memory[i].dev_state = -1;

                        memset( memory[i].cmnd_topic, 0, DB_DATA_LEN );
                        memset( memory[i].stat_topic, 0, DB_DATA_LEN );

                        memset( memory[i].odev_name, 0, DB_DATA_LEN );
                        memset( memory[i].omqtt_topic, 0, DB_DATA_LEN );
                    }

                    break;
                }

                default:
                {
                    log_warn( "default case reached, unknown option %d",
                               to_change[i] );
                    break;
                }
            }

            to_change[i] = -1;
        }

        pthread_mutex_unlock( lock );
        sem_post( mutex );

        /* sleep for 5 seconds */
        usleep(100000*50U);
    }

    return NULL;
}
