/*
 * Database related functions will reside here,
 * for ease.
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
#include <unistd.h>

// local includes
#include "database.h"
#include "config.h"
#include "daemon.h"
#include "ini.h"

#ifdef DEBUG
#include "log.h"
#endif

// pointer to config cfg;
static config *conf;

// pointers for database functions.
static sqlite3 *db_ptr;
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
static int db_callback( void *data, int argc, char **argv, char **azColName );
static int execute_db_query( const char *query );
static int get_db_len();
static int dump_db_entries();
static int insert_db_entry( const char *dev_name, const char *mqtt_topic,
                            const int type, const char *state,
                            const char *valid_commands );
static int delete_db_entry( const char *dev_name, const char *mqtt_topic );
static int update_db_dev_state( const char *dev_name, const char *mqtt_topic,
                                const char *state );


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
 * decrement db_len (like when removing a device)
 *
 * @note this could potentially cause problems, only call when
 * in the critical space of a semaphore.
 */
void decrement_db_count()
{
    --db_len;
}


/**
 * @brief increment db_len (like when adding a new device)
 *
 * @note this could potentially cause problems, only call when
 * in the critical space of a semaphore.
 */
void increment_db_count()
{
    ++db_len;
}

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
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            rv = in;
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
    /* memset the dev_type_str buffer */
    memset( dev_type_str, 0, DEV_TYPE_LEN );

    switch( in )
    {
        case 0:
            strncpy( dev_type_str, DEV_TYPE0, DEV_TYPE0_LEN );
            break;

        case 1:
            strncpy( dev_type_str, DEV_TYPE1, DEV_TYPE1_LEN );
            break;

        case 2:
            strncpy( dev_type_str, DEV_TYPE2, DEV_TYPE2_LEN );
            break;

        case 3:
            strncpy( dev_type_str, DEV_TYPE3, DEV_TYPE3_LEN );
            break;

        case 4:
            strncpy( dev_type_str, DEV_TYPE4, DEV_TYPE4_LEN );
            break;

        case 5:
            strncpy( dev_type_str, DEV_TYPE5, DEV_TYPE5_LEN );
            break;

        case 6:
            strncpy( dev_type_str, DEV_TYPE6, DEV_TYPE6_LEN );
            break;

        case 7:
            strncpy( dev_type_str, DEV_TYPE7, DEV_TYPE7_LEN );
            break;

        default:
            strncpy( dev_type_str, DEV_TYPEX, DEV_TYPEX_LEN );
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

/**
 * @brief Initialize everything for database thread usage.
 *
 * @param cfg the configuration struct for the server.
 * @param db the sqlite3 database pointer.
 * @param sql_buffer the buffer meant for sql statements.
 * @param dat the struct array for items in database.
 * @param to_chng the int array to indicate what may or may need to be changed.
 * @param lck the pthread mutex to prevent any race conditions.
 * @param mtx the semaphore mutex to also prevent any race conditions.
 *
 * @note Returns nonzero upon error.
 */
int initialize_db( config *cfg, sqlite3 *db, char *sql_buffer, db_data *dat,
                   int *to_chng, char *dv_str, pthread_mutex_t *lck,
                   sem_t *mtx )
{
    /*
     * establish a local pointer for sql_buffer,
     * and copy everything in database to memory.
     */
    conf = cfg;
    db_ptr = db;
    sql_buf = sql_buffer;
    memory = dat;
    to_change = to_chng;
    dev_type_str = dv_str;

    /* establish a local pointer for lock semaphore */
    lock = lck;
    mutex = mtx;

    /* Get the count */
    int status = get_db_len();

    if ( status < 0 )
    {
#ifdef DEBUG
        log_error( "Could not get db_len" );
#endif
        return 1;
    }
    else
    {
#ifdef DEBUG
        log_trace( "Found %d entries", db_len );
#endif
    }

    /* Migrate everything to memory */
    status = dump_db_entries();

    if ( status )
    {
#ifdef DEBUG
        log_error( "Could not get dump database entries to memory" );
#endif
        return 1;
    }
    else
    {
#ifdef DEBUG
        log_trace( "Put %d entries into memory", db_len );
#endif
    }

    return 0;

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
 * Information Will truncate if too long.
 */
static int db_callback( void *data, int argc, char **argv, char **azColName )
{
    for ( int i = 0; i < argc; i++ )
    {
        // temporary variable
        int argv_len = strlen(argv[i]);

        if ( strncmp(azColName[i], DEV_NAME, DEV_NAME_LEN) == 0  )
        {
            strncpy( memory[db_counter].dev_name, argv[i],
            (argv_len < DB_DATA_LEN) ? argv_len : DB_DATA_LEN );
        }
        else if ( strncmp(azColName[i], MQTT_TPC, MQTT_TPC_LEN) == 0 )
        {
            strncpy( memory[db_counter].mqtt_topic, argv[i],
            (argv_len < DB_DATA_LEN) ? argv_len : DB_DATA_LEN );
        }
        else if ( strncmp(azColName[i], DEV_TYPE, DV_TYPE_LEN) == 0 )
        {
            memory[db_counter].dev_type = atoi( argv[i] );
        }
        else if ( strncmp(azColName[i], DEV_STATE, DEV_STATE_LEN) == 0 )
        {
            strncpy( memory[db_counter].dev_state, argv[i],
            (argv_len < DV_STATE_LEN) ? argv_len : DV_STATE_LEN );
        }
       else if ( strncmp(azColName[i], VLD_CMDS, VLD_CMDS_LEN) == 0 )
       {
           strncpy( memory[db_counter].valid_cmnds, argv[i],
           (argv_len < VLD_CMDS_LEN) ? argv_len : VLD_CMDS_LEN );
       }
       // a special case to get the count
       else if ( strncmp(azColName[i], COUNT, COUNT_LEN) == 0 )
       {
           /* set the global variable */
           db_len = atoi( argv[i] );
       }
       else
       {
#ifdef DEBUG
           log_error( "Unknown column name %s", azColName[i] );
#endif
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
 * Returns nonzero when an SQL error occurs.
 */
static int execute_db_query( const char *query )
{
    int rv = 0;
    char *errmsg = 0;
    int status = sqlite3_exec( db_ptr, query, db_callback, 0, &errmsg );

    if ( status != SQLITE_OK )
    {
#ifdef DEBUG
        log_warn( "sql error: %s", errmsg );
#endif

        sqlite3_free( errmsg );
        rv = 1;
    }
    else
    {
#ifdef DEBUG
        // update this at some point
        log_debug( "Succesfully ran query '%s' to database",
                   query );
#endif

        rv = 0;
    }

    /* reset db counter */
    db_counter = 0;

    return rv;
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
 * Returns actual length otherwise.
 */
static int get_db_len()
{
    strncpy(sql_buf, GET_LEN_QUERY, GET_LEN_QUERY_LEN );

    int db_ret = execute_db_query( sql_buf );

    /* only if everything is successful */
    if ( db_len >= 0 )
    {
#ifdef DEBUG
        log_trace( "Amount of devices currently: %d", db_len );
#endif
        db_ret = db_len;
    }

    /* memset the sql buffer */
    memset( sql_buf, 0, conf->db_buff );

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
    strncpy ( sql_buf, DB_DUMP_QUERY, DB_DUMP_QUERY_LEN );
    int db_ret = execute_db_query( sql_buf );

    if ( !db_ret )
    {
#ifdef DEBUG
        log_trace( "Successfully dumped %d entries", db_len );
#endif
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
 * @param state is the state in json format.
 * @param valid_commands are the valid commands separated by a comma.
 *
 * @note refer to check_device_type() for valid device type.
 *
 * Returns nonzero upon error.
 */
static int insert_db_entry( const char *dev_name, const char *mqtt_topic,
                            const int type, const char *state,
                            const char *valid_commands )
{
    /* make sure type is valid first */
    if ( check_device_type( type ) == -1 )
    {
#ifdef DEBUG
        log_error( "Invalid device type %d", type );
#endif

        return 2;
    }

    /* make sure there is room for more devices */
    if ( db_len >= conf->max_dev_count )
    {
#ifdef DEBUG
    log_error( "Max devices already reached, ignoring this insert request" );
#endif
        return 3;
    }

    /* Now to do the actual insertion */
    int dv_nm_len = strlen( dev_name );
    int mqtt_tpc_len = strlen( mqtt_topic );
    int type_len = get_digit_count( type );
    int state_len = strlen( state );
    int vld_cmds_len = strlen( valid_commands );

    snprintf( sql_buf, (INSERT_QUERY_LEN + dv_nm_len + mqtt_tpc_len +
              type_len + state_len + vld_cmds_len), INSERT_QUERY, dev_name,
              mqtt_topic, type, state, valid_commands );

    int db_ret = execute_db_query( sql_buf );

    if ( !db_ret )
    {
#ifdef DEBUG
        log_trace( "Successfully inserted device %s", dev_name );
#endif
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
 * @note Returns nonzero upon error.
 */
static int delete_db_entry( const char *dev_name, const char *mqtt_topic )
{
    int dev_nm_len = strlen( dev_name );
    int mqtt_tpc_len = strlen( mqtt_topic );

    snprintf( sql_buf, (DELETE_QUERY_LEN + dev_nm_len + mqtt_tpc_len),
              DELETE_QUERY, dev_name, mqtt_topic );

    int db_ret = execute_db_query( sql_buf );

    if ( !db_ret )
    {
#ifdef DEBUG
        log_trace( "entry removed: %s - %s", dev_name, mqtt_topic );
#endif
    }

     /* memset the sql buffer */
    memset( sql_buf, 0, conf->db_buff );

    return db_ret;
}

/**
 * @brief This has the job of updating a dev_state
 * given both hdev_name and mqtt_topic.
 *
 * @param dev_name the device name of the device that changed states.
 * @param mqtt_topic the mqtt topic of the device that changed states.
 * @param new_state the new state in json format.
 *
 * @note Returns nonzero upon error.
 *
 */
static int update_db_dev_state( const char *dev_name, const char *mqtt_topic,
                                const char *state )
{
    int dv_nm_len = strlen( dev_name );
    int mqtt_tpc_len = strlen( mqtt_topic );
    int state_len = strlen( state );

    snprintf( sql_buf, (STATE_QUERY_LEN + dv_nm_len + mqtt_tpc_len +
              state_len), STATE_QUERY, state, dev_name, mqtt_topic );

    int db_ret = execute_db_query( sql_buf );

    if ( !db_ret )
    {
#ifdef DEBUG
        log_trace( "entry %s state updated", dev_name );
#endif
    }

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
    usleep( 100000 * 50 );

    while( 1 )
    {
        sem_wait( mutex );
        pthread_mutex_lock( lock );

        /*
         * Critical Section
         */

        pthread_mutex_unlock( lock );
        sem_post( mutex );

        /* sleep for specified amount of time */
        usleep( 100000 * 10 * SLEEP_DELAY );
    }
}
