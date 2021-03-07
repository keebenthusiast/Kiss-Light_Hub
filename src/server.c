/*
 * The server which has the job of handling mqtt publishes and subscriptions,
 * and whatever a user may request a connected mqtt device to handle.
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
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>

// socket-related includes
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/wait.h>
#include <fcntl.h>

// local includes
#include "server.h"
#include "config.h"
#include "database.h"
#include "daemon.h"
#include "mqtt.h"
#include "statejson.h"

#ifdef DEBUG
#include "log.h"
#endif

// pointer to config cfg;
static config *conf;

// pointers for database functions.
static db_data *memory;
static int *to_change;

// pointers for various server buffers and fds
static char **server_buffer;
static struct pollfd *clientfds;

// mqtt buffers
static char *topic;
static char *app_msg;

// mqtt client pointer
struct mqtt_client *cl;

// System pointers
static pthread_mutex_t *lock;
static sem_t *mutex;

/*
 * When exiting, close server's socket,
 * using this variable
 */
static unsigned int closeSocket = 0;

/* local prototypes as needed */
static int add_device( const char *dv_name, const char *mqtt_tpc,
                       const int dv_type, const char *vld_cmds );
static int delete_device( char *dv_name );
static int update_device( const char *req, const char *dev_name,
                          const char *arg, char *buf, int *n );
static int change_dev_state( const char *dv_name, const char *cmd, char *msg );
static int toggle_dev_power( const char *dv_name, const char *msg );
static void dump_devices( char *buf, int *n );
static int get_dev_state( const char *dv_name, char *buf, int *n );

/*******************************************************************************
 * Non-specific server-related initializations will reside here.
 * such as: sharing pointers to some buffers
 *
 * Also where misc functions will reside as well.
 ******************************************************************************/

/**
 * @brief assign buffers and mutex semaphores to the server.
 *
 * @param srvr_buf The server's buffer.
 * @param str_buf The parser's buffer.
 * @param tpc the topic buffer.
 * @param application_msg the application message buffer.
 * @param data the struct array of database entries.
 * @param cfg the configuration struct
 * @param to_chng the int array to indicate what is to be updated
 * @param lck the pthread mutex lock
 * @param mtx the semaphore mutex lock
 *
 */
void assign_buffers( char **srvr_buf, char *tpc, char *application_msg,
                     db_data *data, config *cfg, int *to_chng,
                     pthread_mutex_t *lck, sem_t *mtx,
                     struct pollfd *cfds )
{
    server_buffer = srvr_buf;
    topic = tpc;
    app_msg = application_msg;
    memory = data;
    to_change = to_chng;
    conf = cfg;
    lock = lck;
    mutex = mtx;
    clientfds = cfds;
}

/**
 * @brief Will convert string input to uppercase
 *
 * @param str the string to be converted to uppercase.
 *
 * @note will return if str equals one of the suffix constants.
 */
static void convert_to_upper( char *str )
{
    /* Convert to upper case */
    for ( int i = 0; str[i] != '\0'; i++ )
    {
          if ( str[i] >= 'a' && str[i] <= 'z' )
          {
              str[i] = toupper(str[i]);
          }
    }
}

/**
 * @brief Prepare full mqtt topic for the topic buffer.
 *
 * @param prefix the prefix of the full mqtt topic, should be 'stat/' for
 * subscriptions, or 'cmnd/' for sending commands, but could be used for
 * anything else if desired.
 * @param tpc the short mqtt topic, the device's identifying mqtt topic.
 * @param suffix the suffix of the full mqtt topic, should be a commmand,
 * or if subscribing, use '/RESULT'.
 *
 * @note the topic buffer gets memsetted, so use carefully. It is also
 * not thread safe, so make sure to implement semaphores when using.
 */
void prepare_topic( const char *prefix, const char *tpc,
                    char *suffix )
{
    /*
     * Do a quick check to make sure it
     * is not a pre-existing suffix string.
     *
     * if it is skip.
     */

    if ( ! (strncmp(suffix, RESULT, RESULT_LEN) == 0)
        || ! (strncmp(suffix, STATE, STATE_LEN) == 0) )
    {
        /* convert suffix to_upper first */
        convert_to_upper( suffix );
    }

    /* memset the buffer just in case */
    memset( topic, 0, conf->topic_buff );

    /* create full topic */
    int pfx = strlen( prefix );
    int topc = strlen( tpc );
    int sfx = strlen( suffix );

    if ( suffix[0] != '/' )
    {
        /*
         * verify total length,
         * this can truncate so take care.
         *
         * I increment the total length + 1
         * to take into account the full length.
         */
        int len = ((pfx + topc + sfx + 2) < conf->topic_buff ) ?
                    pfx + topc + sfx + 2 : conf->topic_buff;

        snprintf( topic, len, "%s%s/%s", prefix, tpc, suffix );
    }
    else
    {
        int len = ((pfx + topc + sfx + 1) < conf->topic_buff ) ?
                    pfx + topc + sfx + 1 : conf->topic_buff;

        snprintf( topic, len, "%s%s%s", prefix, tpc, suffix );
    }
}

/**
 * @brief Verify that the command is valid given a list
 * of valid commands associated with a device.
 *
 * @param input the input command to compare against the cmnds.
 * @param cmnds the list of commands to be compared with, delimited by
 * a comma.
 *
 * @note Returns nonzero upon error.
 */
static int verify_command( const char *input, const char *cmnds )
{
    int rv = 0;

    /* create temporary copy to keep original in tact. */
    char tmp_cmnds[DB_CMND_LEN];
    strncpy( tmp_cmnds, cmnds, strlen(cmnds) + 1 );

    char *tok;
    tok = strtok(tmp_cmnds, ",");

    while ( tok != 0 )
    {
        if ( strncasecmp(input, tok, strlen(input) + 1) == 0 )
        {
            rv = 0;
            break;
        }

        tok = strtok( 0, "," );

        /* if we made it this far, it is likely not a valid command */
        if ( tok == 0 )
        {
            rv = 1;
        }
    }

    return rv;
}

/*******************************************************************************
 * Everything related to message parsing will reside here.
 ******************************************************************************/

/**
 * @brief Check protocol version.
 *
 * @param buf the buffer that contains the protocol version.
 *
 * @note example being: KL/0.3 .
 * Return protocol_version if valid
 * or -1.0 if invalid.
 */
static float get_protocol_version(char *buf)
{
    float protocol = -1.0;

    /*
     * check to make sure buf isn't empty and first part equals KL or
     * something along those lines
     */
    if ( strncasecmp( buf, "KL", 2) != 0 )
    {
        return protocol;
    }

    /* Now extract the protocol version itself */
    if ( strncmp( buf, "kl", 2) == 0 )
    {
        sscanf( buf, "kl/%f", &protocol );
    }
    else if ( strncmp( buf, "Kl", 2) == 0 )
    {
        sscanf( buf, "Kl/%f", &protocol );
    }
    else if ( strncmp( buf, "kL", 2 ) == 0 )
    {
        sscanf( buf, "kL/%f", &protocol );
    }
    else
    {
        sscanf( buf, "KL/%f", &protocol );
    }

    /* make sure the protocol version is correct */
    if ( protocol <= 0.0 )
    {
        return -1.0;
    }

    return protocol;
}

/**
 * @brief Parse whatever the client sent in,
 * Do the task if applicable,
 * And send over the proper response.
 *
 * @param buf the buffer to be analyzed, THEN MODIFIED.
 * @param n the buffer length, modified when buf is modified.
 *
 * @note Returns -1 when a user requests to quit, Returns 0 otherwise.
 */
static int parse_server_request( char *buf, int *n )
{
    /* return value, should be zero unless user wants to quit */
    int rv = 0;

    /* Create tmporary buffer */
    char tmp[conf->buffer_size];
    strncpy( tmp, buf, strlen(buf) + 1 );

    /* clean buffer for reuse */
    memset( buf, 0, conf->buffer_size );

    /* ready to analyze request args passed in */
    int arg_count = 0;
    char req_args[ARG_LEN][ARG_BUF_LEN];
    char *tok;
    tok = strtok( tmp, " " );

    /* Empty buffer to prevent problems */
    for ( int i = 0; i < ARG_LEN; i++ )
    {
        memset( req_args[i], 0, ARG_BUF_LEN );
    }

    /* now process args */
    while ( tok != 0 && arg_count < ARG_LEN )
    {
        strncpy( req_args[arg_count], tok, strlen(tok) + 1 );

        tok = strtok( 0, " " );

        arg_count++;
    }

    // TRANSMIT custom_topic custom_message KL/version#
    if ( strncasecmp(req_args[0], TRANSMIT, TRANSMIT_LEN) == 0 )
    {
#ifdef DEBUG
        for ( int i = 0; i < arg_count; i++ )
        {
            printf( "%s\n", req_args[i] );
        }
#endif

        /* Verify arg len */
        if ( arg_count < TRANSMIT_ARG )
        {
            *n = snprintf( buf, MESSAGE_409_LEN, MESSAGE_409, KL_VERSION );

            return rv;
        }

        /* verify that protocol version is found */
        if( get_protocol_version(req_args[arg_count - 1]) < 0.1 )
        {
            *n = snprintf( buf, MESSAGE_406_LEN, MESSAGE_406, KL_VERSION );

            return rv;
        }

        /* execute request */
        mqtt_publish( cl, req_args[1], req_args[2], strlen(req_args[2]),
                      MQTT_PUBLISH_QOS_0 );

        /* verify results */
        if ( cl->error != MQTT_OK )
        {
#ifdef DEBUG
            log_warn( "mqtt error: %s", mqtt_error_str(cl->error) );
#endif

            int len = strlen(mqtt_error_str(cl->error)) + MESSAGE_500_LEN;
            *n = snprintf( buf, len, MESSAGE_500, KL_VERSION,
                      mqtt_error_str(cl->error) );
        }
        else
        {
            int len = strlen(req_args[1]) + strlen(req_args[2]) +
                      MESSAGE_205_LEN;
            *n = snprintf( buf, len, MESSAGE_205, KL_VERSION,
                      req_args[1], req_args[2] );
        }
    }
    // TOGGLE dev_name KL/version#
    else if ( strncasecmp(req_args[0], TOGGLE, TOGGLE_LEN) == 0 )
    {
#ifdef DEBUG
        for ( int i = 0; i < arg_count; i++ )
        {
            printf( "%s\n", req_args[i] );
        }
#endif

        /* Verify arg len */
        if ( arg_count < TOGGLE_ARG )
        {
            *n = snprintf( buf, MESSAGE_409_LEN, MESSAGE_409, KL_VERSION );

            return rv;
        }

        /* verify that protocol version is found */
        if( get_protocol_version(req_args[arg_count - 1]) < 0.1 )
        {
            *n = snprintf( buf, MESSAGE_406_LEN, MESSAGE_406, KL_VERSION );

            return rv;
        }

        /* execute request */
        int status = toggle_dev_power( req_args[1], TOGGLE );

        /* verify results */
        if ( status )
        {
            int len = strlen(req_args[1]) + MESSAGE_404_LEN;
            *n = snprintf( buf, len, MESSAGE_404, KL_VERSION, req_args[1] );
        }
        else
        {
            int len = strlen(req_args[1]) + MESSAGE_200_LEN;
            *n = snprintf( buf, len, MESSAGE_200, KL_VERSION, req_args[1] );
        }
    }
    // SET dev_name command message KL/version#
    else if ( strncasecmp(req_args[0], SET_REQ, SET_REQ_LEN) == 0 )
    {
#ifdef DEBUG
        for ( int i = 0; i < arg_count; i++ )
        {
            printf( "%s\n", req_args[i] );
        }
#endif

        /* Verify arg len */
        if ( arg_count < SET_ARG )
        {
            *n = snprintf( buf, MESSAGE_409_LEN, MESSAGE_409, KL_VERSION );

            return rv;
        }

        /* verify that protocol version is found */
        if( get_protocol_version(req_args[arg_count - 1]) < 0.1 )
        {
            *n = snprintf( buf, MESSAGE_406_LEN, MESSAGE_406, KL_VERSION );

            return rv;
        }

        /* execute request */
        int status = change_dev_state( req_args[1], req_args[2], req_args[3] );

        /* verify results */
        if ( status == 2)
        {
            int len = strlen(req_args[2]) + MESSAGE_405_LEN;
            *n = snprintf( buf, len, MESSAGE_405, KL_VERSION, req_args[2] );
        }
        else if ( status == 1 )
        {
            int len = strlen(req_args[1]) + MESSAGE_404_LEN;
            *n = snprintf( buf, len, MESSAGE_404, KL_VERSION, req_args[1] );
        }
        else
        {
            int len = strlen(req_args[1]) + strlen(req_args[2]) +
                      strlen(req_args[3]) + MESSAGE_201_LEN;
            *n = snprintf( buf, len, MESSAGE_201, KL_VERSION, req_args[1],
                           req_args[2], req_args[3] );
        }
    }
    // ADD dev_name mqtt_topic dev_type <valid_cmnds> KL/version#
    else if ( strncasecmp(req_args[0], ADD_REQ, ADD_REQ_LEN) == 0 )
    {
#ifdef DEBUG
        for ( int i = 0; i < arg_count; i++ )
        {
            printf( "%s\n", req_args[i] );
        }
#endif
        int id = atoi( req_args[3] );

        /* Verify arg len */
        switch( id )
        {
            case 1:
            case 7:
            {
                if ( arg_count < ADD_ARGB )
                {
                    *n = snprintf( buf, MESSAGE_409_LEN,
                                  MESSAGE_409, KL_VERSION );

                    return rv;
                }

                break;
            }

            default:
            {
                if ( arg_count < ADD_ARGA )
                {
                    *n = snprintf( buf, MESSAGE_409_LEN,
                                  MESSAGE_409, KL_VERSION );

                    return rv;
                }

                break;
            }
        }

        /* verify that protocol version is found */
        if( get_protocol_version(req_args[arg_count - 1]) < 0.1 )
        {
            *n = snprintf( buf, MESSAGE_406_LEN, MESSAGE_406, KL_VERSION );

            return rv;
        }

        /* execute request */
        int status;
        switch( id )
        {
            case 1:
            case 7:
            {
                status = add_device( req_args[1], req_args[2],
                                     id, req_args[4] );
                break;
            }

            default:
            {
                status = add_device( req_args[1], req_args[2], id, NULL );
                break;
            }
        }

        /* verify results */
        if ( status )
        {
            int len = strlen(req_args[1]) + MESSAGE_403_LEN;
            *n = snprintf( buf, len, MESSAGE_403, KL_VERSION, req_args[1] );
        }
        else if ( status > 1 )
        {
            int len = strlen(req_args[1]) + MESSAGE_408_LEN;
            *n = snprintf( buf, len, MESSAGE_408, KL_VERSION, req_args[1] );
        }
        else
        {
            int len = strlen(req_args[1]) + MESSAGE_202_LEN;
            *n = snprintf( buf, len, MESSAGE_202, KL_VERSION, req_args[1] );
        }
    }
    // DELETE dev_name KL/version#
    else if ( strncasecmp(req_args[0], DEL_REQ, DEL_REQ_LEN) == 0 )
    {
#ifdef DEBUG
        for ( int i = 0; i < arg_count; i++ )
        {
            printf( "%s\n", req_args[i] );
        }
#endif

        /* Verify arg len */
        if ( arg_count < DELETE_ARG )
        {
            *n = snprintf( buf, MESSAGE_409_LEN, MESSAGE_409, KL_VERSION );

            return rv;
        }

        /* verify that protocol version is found */
        if( get_protocol_version(req_args[arg_count - 1]) < 0.1 )
        {
            *n = snprintf( buf, MESSAGE_406_LEN, MESSAGE_406, KL_VERSION );

            return rv;
        }

        /* execute request */
        int status = delete_device( req_args[1] );

        /* verify results */
        if ( status )
        {
            int len = strlen(req_args[1]) + MESSAGE_402_LEN;
            *n = snprintf( buf, len, MESSAGE_402, KL_VERSION, req_args[1] );
        }
        else
        {
            int len = strlen(req_args[1]) + MESSAGE_203_LEN;
            *n = snprintf( buf, len, MESSAGE_203, KL_VERSION, req_args[1] );
        }
    }
    // UPDATE NAME old_dev_name new_dev_name KL/version#
    // UPDATE TOPIC dev_name new_mqtt_topic KL/version#
    // UPDATE STATE dev_name KL/version#
    else if ( strncasecmp(req_args[0], UPDATE_REQ, UPDATE_REQ_LEN) == 0 )
    {
#ifdef DEBUG
        for ( int i = 0; i < arg_count; i++ )
        {
            printf( "%s\n", req_args[i] );
        }
#endif

        /* Verify arg len */
        if ( arg_count < UPDATE_ARG )
        {
            *n = snprintf( buf, MESSAGE_409_LEN, MESSAGE_409, KL_VERSION );

            return rv;
        }

        /* verify that protocol version is found */
        if( get_protocol_version(req_args[arg_count - 1]) < 0.1 )
        {
            *n = snprintf( buf, MESSAGE_406_LEN, MESSAGE_406, KL_VERSION );

            return rv;
        }

        /* execute request */
        int status = update_device( req_args[1], req_args[2], req_args[3],
                                    buf, n );

        /* verify results */
        if ( status == 2 )
        {
            int len = strlen(req_args[1]) + MESSAGE_405_LEN;
            *n = snprintf( buf, len, MESSAGE_405, KL_VERSION, req_args[1] );
        }
        else if ( status == 1 )
        {
            int len = strlen(req_args[2]) + MESSAGE_404_LEN;
            *n = snprintf( buf, len, MESSAGE_404, KL_VERSION, req_args[2] );
        }
    }
    // LIST KL/version#
    else if ( strncasecmp(req_args[0], LIST, LIST_LEN) == 0 )
    {
#ifdef DEBUG
        for ( int i = 0; i < arg_count; i++ )
        {
            printf( "%s\n", req_args[i] );
        }
#endif

        /* Verify arg len */
        if ( arg_count < LIST_ARG )
        {
            *n = snprintf( buf, MESSAGE_409_LEN, MESSAGE_409, KL_VERSION );

            return rv;
        }

        /* verify that protocol version is found */
        if( get_protocol_version(req_args[arg_count - 1]) < 0.1 )
        {
            *n = snprintf( buf, MESSAGE_406_LEN, MESSAGE_406, KL_VERSION );

            return rv;
        }

        /* show the current devices to the client */
        dump_devices( buf, n );
    }
    // STATUS dev_name KL/version#
    else if ( strncasecmp(req_args[0], STATUS, STATUS_LEN) == 0 )
    {
#ifdef DEBUG
        for ( int i = 0; i < arg_count; i++ )
        {
            printf( "%s\n", req_args[i] );
        }
#endif

        /* Verify arg len */
        if ( arg_count < STATUS_ARG )
        {
            *n = snprintf( buf, MESSAGE_409_LEN, MESSAGE_409, KL_VERSION );

            return rv;
        }

        /* verify that protocol version is found */
        if( get_protocol_version(req_args[arg_count - 1]) < 0.1 )
        {
            *n = snprintf( buf, MESSAGE_406_LEN, MESSAGE_406, KL_VERSION );

            return rv;
        }

        int status = get_dev_state( req_args[1], buf, n );

        if ( status )
        {
            int len = strlen(req_args[1]) + MESSAGE_404_LEN;
            *n = snprintf(buf, len, MESSAGE_404, KL_VERSION, req_args[1] );
        }
    }
    // allow the client to disconnect
    else if ( strncasecmp(req_args[0], QA, QA_LEN) == 0
           || strncasecmp(req_args[0], QB, QB_LEN) == 0 )
    {
        *n = snprintf( buf, MESSAGE_207_LEN, MESSAGE_207, KL_VERSION );

        rv = -1;
    }
    // Something else was passed in
    else
    {
#ifdef DEBUG
        printf( "passed in: %s\n", req_args[0] );
#endif

        *n = snprintf( buf, MESSAGE_400_LEN, MESSAGE_400, KL_VERSION );
    }

    return rv;
}

/*******************************************************************************
 * Everything related to the server will reside here.
 ******************************************************************************/

/**
 * @brief Function that adds a device to memory,
 * then eventually the database.
 *
 * @param dv_name the device name of the new device
 * @param mqtt_tpc the mqtt_topic associated with the new device
 * @param dv_type the type of device the new device is
 *
 * @note
 * Returns 2 for device already exists,
 * returns 1 for failure to add device
 * (usually because too many devices), returns 0 otherwise.
 */
static int add_device( const char *dv_name, const char *mqtt_tpc,
                       const int dv_type, const char *vld_cmds )
{
    int rv = 1; /* return value, assume too many devices */
    int dup = 0; /* assume no dupes initially */
    int loc = -1; /* assume no free space initially */

    /* make sure dev_id is correct, return if not */
    if ( dv_type > DEV_TYPE_MAX || dv_type < 0 )
    {
        return rv;
    }

    sem_wait( mutex );
    pthread_mutex_lock( lock );

    /* Scan for an empty spot, check for duplicates */
    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        /* Check for a duplicate */
        if ( strncasecmp( memory[i].dev_name, dv_name,
             strlen(dv_name)) == 0 )
        {
            /* duplicate found */
            dup = 1;
            rv = 2;
            break;
        }

        /* take the first found free spot */
        if ( memory[i].dev_name[0] == '\0' && loc == -1 )
        {
            rv = 0;
            loc = i;
        }
    }

    /* only add if there does not exist a duplicate */
    if ( !dup && rv == 0 )
    {
        /* Copy necessary information */
        strncpy( memory[loc].dev_name, dv_name, strlen(dv_name) + 1 );
        strncpy( memory[loc].mqtt_topic, mqtt_tpc, strlen(mqtt_tpc) + 1 );
        memory[loc].dev_type = dv_type;
        strncpy( memory[loc].dev_state, DEV_STATE_TMPL, DV_STATE_TMPL_LEN );

        /*
         * Fill in valid commands, depending on dev_type
         */
        switch( memory[loc].dev_type )
        {
            case 0:
            {
                strncpy( memory[loc].valid_cmnds, DEV_TYPE0_CMDS,
                         DEV_TYPE0_CMDS_LEN );
                break;
            }

            case 1:
            {
                int count = atoi( vld_cmds );
                powerstrip_cmnd_cat( memory[loc].valid_cmnds, count );
                break;
            }

            case 2:
            {
                strncpy( memory[loc].valid_cmnds, DEV_TYPE2_CMDS,
                         DEV_TYPE2_CMDS_LEN );
                break;
            }

            case 3:
            {
                strncpy( memory[loc].valid_cmnds, DEV_TYPE3_CMDS,
                         DEV_TYPE3_CMDS_LEN );
                break;
            }

            case 4:
            {
                strncpy( memory[loc].valid_cmnds, DEV_TYPE4_CMDS,
                         DEV_TYPE4_CMDS_LEN );
                break;
            }

            case 5:
            {
                strncpy( memory[loc].valid_cmnds, DEV_TYPE5_CMDS,
                         DEV_TYPE5_CMDS_LEN );
                break;
            }

            case 6:
            {
                strncpy( memory[loc].valid_cmnds, DEV_TYPE6_CMDS,
                         DEV_TYPE6_CMDS_LEN );
                break;
            }

            case 7:
            {
                strncpy( memory[loc].valid_cmnds, vld_cmds,
                         strlen(vld_cmds) + 1 );
                break;
            }
        }

        /* subscribe to this new device */
        prepare_topic( STAT, memory[loc].mqtt_topic, (char *)RESULT );
        mqtt_subscribe( cl, topic, 0 );

        /* request the current state if at all possible */
        prepare_topic( CMND, memory[loc].mqtt_topic, (char *)STATE );
        mqtt_publish( cl, topic, "", 0, MQTT_PUBLISH_QOS_0 );

        /* add this device to database! */
        to_change[loc] = 4;

        /* increment database count */
        increment_db_count();

        /* set return value to success */
        rv = 0;
    }

    pthread_mutex_unlock( lock );
    sem_post( mutex );

    return rv;
}

/**
 * @brief Function that adds a device to memory,
 * then eventually the database.
 *
 * @param dv_name the device name of the device to remove
 *
 * @note Returns 1 for failure to delete device
 * (usually because it does not exist), returns 0 otherwise.
 */
static int delete_device( char *dv_name )
{
    int rv = 1; /* return value */

    sem_wait( mutex );
    pthread_mutex_lock( lock );

    /* scan for a dev_name match */
    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        if ( strncasecmp(memory[i].dev_name, dv_name, strlen(dv_name)) == 0 )
        {
            /* Copy current information to old, and memset */
            strncpy( memory[i].odev_name, memory[i].dev_name,
                     strlen(memory[i].dev_name) + 1 );
            strncpy( memory[i].omqtt_topic, memory[i].mqtt_topic,
                     strlen(memory[i].mqtt_topic) + 1 );
            memset( memory[i].dev_name, 0, DB_DATA_LEN );
            memset( memory[i].mqtt_topic, 0, DB_DATA_LEN );

            /* unsubscribe from this device */
            prepare_topic( STAT, memory[i].omqtt_topic, (char *)RESULT );
            mqtt_unsubscribe( cl, topic );

            /* delete this device from database! */
            to_change[i] = 5;

            /* decrement database count */
            decrement_db_count();

            /* set return value to success */
            rv = 0;

            /* all done */
            break;
        }
    }

    pthread_mutex_unlock( lock );
    sem_post( mutex );

    return rv;
}

/**
 * @brief Change either device's name, topic, or just get the full
 * state of the device of interest.
 *
 *
 * @param req the request, expected to be "NAME", "TOPIC", or "STATE".
 * @param dev_name the dev_name of the device to be modified.
 * @param arg the arg, could be new dev_name, new mqtt_topic, or just NULL.
 * @param buf the buffer for the client, to make a tailor made response. In
 * other words, THIS GETS MODIFIED.
 * @param n the buffer length var. this also gets modified when buf gets
 * modifed.
 *
 * @note Returns 1 for no such device, returns 2 for invalid request,
 * returns 0 otherwise.
 */
static int update_device( const char *req, const char *dev_name,
                          const char *arg, char *buf, int *n )
{
    int rv = 0; /* return value */
    int loc = -1; /* location of a device match */

    /* check if req is invalid */
    if ( strncasecmp( req, UPDATE_A, UPDATE_A_LEN) != 0
      && strncasecmp( req, UPDATE_B, UPDATE_B_LEN) != 0
      && strncasecmp( req, UPDATE_C, UPDATE_C_LEN) != 0 )
    {
        return 2;
    }

    sem_wait( mutex );
    pthread_mutex_lock( lock );

    /* scan for a dev_name match */
    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        if ( strncasecmp(memory[i].dev_name, dev_name, strlen(dev_name)) == 0 )
        {
            loc = i;
            break;
        }

        /* No matches at all? all done */
        if ( i == (conf->max_dev_count - 1) )
        {
            rv = 1;
        }
    }

    if ( !rv )
    {
        /* Update dev_name */
        if ( strncasecmp(req, UPDATE_A, UPDATE_A_LEN) == 0 )
        {
            if ( memory[loc].odev_name[0] == '\0' )
            {
                strncpy( memory[loc].odev_name, dev_name, strlen(dev_name) );
            }
            memset( memory[loc].dev_name, 0, DB_DATA_LEN );
            strncpy( memory[loc].dev_name, arg, strlen(arg) );

            /* set respective to_change value as needed */
            switch( to_change[loc] )
            {
                case -1:
                case 1:
                    to_change[loc] = 1;
                    break;

                case 0:
                {
                    strncpy( memory[loc].omqtt_topic,
                             memory[loc].mqtt_topic,
                             strlen(memory[loc].mqtt_topic) );

                    /* stage to a full update */
                    to_change[loc] = 3;
                    break;
                }

                case 2:
                {
                    /* stage to a full update */
                    to_change[loc] = 3;
                    break;
                }

                default:
                    break;
            }

            int len = strlen(memory[loc].odev_name) + strlen(arg) +
                      MESSAGE_208_LEN;
            *n = snprintf( buf, len, MESSAGE_208, KL_VERSION,
                      memory[loc].odev_name, arg );
        }
        /* Update mqtt topic */
        else if ( strncasecmp(req, UPDATE_B, UPDATE_B_LEN) == 0 )
        {
            char tmp[DB_DATA_LEN];
            memset( tmp, 0, DB_DATA_LEN );

            if ( memory[loc].omqtt_topic[0] == '\0' )
            {
                /* Copy current mqtt topic to old */
                strncpy( memory[loc].omqtt_topic, memory[loc].mqtt_topic,
                         strlen(memory[loc].mqtt_topic) );
            }
            else
            {
                /* copy whatever is the current mqtt topic to tmp */
                strncpy( tmp, memory[loc].mqtt_topic,
                         strlen(memory[loc].mqtt_topic) );
            }

            /* has not been called yet this db_updater cycle */
            if ( tmp[0] == '\0')
            {
                /* Set the new topic using mqtt */
                prepare_topic( CMND, memory[loc].omqtt_topic,
                               (char *)MQTT_UPDATE );
                mqtt_publish( cl, topic, arg, strlen(arg),
                              MQTT_PUBLISH_QOS_0 );

                /* unsub from the old topic */
                prepare_topic( STAT, memory[loc].omqtt_topic, (char *)RESULT );
                mqtt_unsubscribe( cl, topic );
            }
            /* has been called once this db_updater cycle already */
            else
            {
                /* Set the new topic using mqtt */
                prepare_topic( CMND, tmp, (char *)MQTT_UPDATE );
                mqtt_publish( cl, topic, arg, strlen(arg),
                              MQTT_PUBLISH_QOS_0 );

                /* unsub from the old topic */
                prepare_topic( STAT, tmp, (char *)RESULT );
                mqtt_unsubscribe( cl, topic );
            }

            /* memset mqtt_topic and copy new topic over */
            memset( memory[loc].mqtt_topic, 0, DB_DATA_LEN );
            strncpy( memory[loc].mqtt_topic, arg, strlen(arg) );

            /* finally subscribe to the new topic */
            prepare_topic( STAT, arg, (char *)RESULT );
            mqtt_subscribe( cl, topic, 0 );

            /* set respective to_change value as needed */
            switch( to_change[loc] )
            {
                case -1:
                case 2:
                    to_change[loc] = 2;
                    break;

                case 0:
                {
                    strncpy( memory[loc].odev_name,
                             memory[loc].dev_name,
                             strlen(memory[loc].dev_name) );

                    /* stage to a full update */
                    to_change[loc] = 3;
                    break;
                }

                case 1:
                {
                    /* stage to a full update */
                    to_change[loc] = 3;
                    break;
                }

                default:
                    break;
            }

            int len = strlen(memory[loc].dev_name) + strlen(arg) +
                      MESSAGE_209_LEN;
            *n = snprintf( buf, len, MESSAGE_209, KL_VERSION,
                      memory[loc].dev_name, arg );
        }
        /* Update dev_state via mqtt */
        else if ( strncasecmp(req, UPDATE_C, UPDATE_C_LEN) == 0 )
        {
            prepare_topic( CMND, memory[loc].mqtt_topic, (char *)STATE );
            mqtt_publish( cl, topic, "", 0, MQTT_PUBLISH_QOS_0 );

            /* set respective to_change value as needed */
            switch( to_change[loc] )
            {
                case -1:
                case 0:
                    to_change[loc] = 0;
                    break;

                case 1:
                {
                    strncpy( memory[loc].omqtt_topic,
                             memory[loc].mqtt_topic,
                             strlen(memory[loc].mqtt_topic) );

                    /* stage to a full update */
                    to_change[loc] = 3;
                    break;
                }

                case 2:
                {
                    strncpy( memory[loc].odev_name,
                             memory[loc].dev_name,
                             strlen(memory[loc].dev_name) );

                    /* stage to a full update */
                    to_change[loc] = 3;
                    break;
                }

                default:
                    break;
            }

            int len = strlen(memory[loc].dev_name) + MESSAGE_210_LEN;
            *n = snprintf( buf, len, MESSAGE_210, KL_VERSION,
                      memory[loc].dev_name );
        }
    }

    pthread_mutex_unlock( lock );
    sem_post( mutex );

    return rv;
}

/**
 * @brief Change device's state.
 *
 * @param dv_name the device name to change.
 * @param cmd the device's command to be sent.
 * @param msg the message to change dev state.
 *
 * @note Returns 1 for no such device, Returns 2 for invalid mqtt
 * command, otherwise returns 0.
 */
static int change_dev_state( const char *dv_name, const char *cmd, char *msg )
{
    int rv = 0; /* return value */
    int loc = -1; /* location of a device match */

    /* Only at this point is memory going to be accessed. */
    sem_wait( mutex );
    pthread_mutex_lock( lock );

    /* scan for a dev_name match */
    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        if ( strncasecmp(memory[i].dev_name, dv_name, strlen(dv_name)) == 0 )
        {
            loc = i;
            break;
        }

        /* No matches at all? all done */
        if ( i == (conf->max_dev_count - 1) )
        {
            rv = 1;
        }
    }

    /* only continue if a device is found */
    if ( !rv )
    {
        /* verify cmd is acceptable for the device */
        if ( verify_command( cmd, memory[loc].valid_cmnds ) )
        {
            rv = 2;
        }

        /* if the command is valid, ship it! */
        if ( rv == 0 )
        {
            prepare_topic( CMND, memory[loc].mqtt_topic, (char *)cmd );
            mqtt_publish( cl, topic, msg, strlen(msg),
                          MQTT_PUBLISH_QOS_0 );
        }
    }

    pthread_mutex_unlock( lock );
    sem_post( mutex );

    return rv;
}

/**
 * @brief Change device's power state.
 *
 * @param dv_name the device name to change.
 * @param cmd the device's command to be sent.
 * @param msg the message to change dev state.
 *
 * @note Returns 1 for no such device, otherwise returns 0.
 */
static int toggle_dev_power( const char *dv_name, const char *msg )
{
    int rv = 0; /* return value */
    int loc = -1; /* location of a device match */

    /* Only at this point is memory going to be accessed. */
    sem_wait( mutex );
    pthread_mutex_lock( lock );

    /* scan for a dev_name match */
    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        if ( strncasecmp(memory[i].dev_name, dv_name, strlen(dv_name)) == 0 )
        {
            loc = i;
            break;
        }

        /* No matches at all? all done */
        if ( i == (conf->max_dev_count - 1) )
        {
            rv = 1;
        }
    }

    /* only continue if a device is found */
    if ( !rv )
    {
        char cmd[DB_CMND_LEN];
        memset( cmd, 0, DB_CMND_LEN );

        /* if a powerstrip set to POWER0 */
        if ( memory[loc].dev_type == 1 )
        {
            snprintf( cmd, DEV_TYPE1B_CMD_LEN, DEV_TYPE1B_CMD, 0 );
            prepare_topic( CMND, memory[loc].mqtt_topic, cmd );
        }
        else
        {
            strncpy(cmd, DEV_TYPE0_CMDS, DEV_TYPE0_CMDS_LEN );
            prepare_topic( CMND, memory[loc].mqtt_topic, cmd );
        }

        mqtt_publish( cl, topic, msg, strlen(msg),
                      MQTT_PUBLISH_QOS_0 );
    }

    pthread_mutex_unlock( lock );
    sem_post( mutex );

    return rv;
}

/**
 * @brief Function that prints devices in memory when requested to list
 * devices by client.
 *
 * @param buf the buffer for the client, to make a tailor made response. In
 * other words, THIS GETS MODIFIED.
 * @param n the buffer length var. this also gets modified when buf gets
 * modifed.
 *
 * @note This uses semaphores since it access memory to satisfy the request.
 */
static void dump_devices( char *buf, int *n )
{
    /* create new temporary buffers */
    char tmp[DB_CMND_LEN];
    char tmp_msg[conf->buffer_size];
    char *dv_type;

    /* memset buffers to prevent issues down the road */
    memset( tmp, 0, DB_CMND_LEN );
    memset( tmp_msg, 0, DB_CMND_LEN );

    /* create first part of message */
    int len = MESSAGE_204_LEN + get_digit_count( get_current_entry_count() );
    snprintf( tmp, len, MESSAGE_204, KL_VERSION, get_current_entry_count() );
    strncat( tmp_msg, tmp, len );

    /* copy the current len */
    *n = len;

    sem_wait( mutex );
    pthread_mutex_lock( lock );

    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        /* Empty, move on */
        if ( memory[i].dev_name[0] == '\0' )
        {
            continue;
        }

        dv_type = device_type_to_str( memory[i].dev_type );

        len = (DUMP_204_LEN + strlen(memory[i].dev_name) +
                  strlen(memory[i].mqtt_topic) + strlen(dv_type));

        snprintf( tmp, len, DUMP_204, memory[i].dev_name,
                  memory[i].mqtt_topic, dv_type );

        strncat( tmp_msg, tmp, len );

        /* increment total len */
        *n += len;
        *n -= 1;
    }

    /* create a terminating character for this. */
    strncat( tmp_msg, ".\n", 3 );

    /* increment total len */
    *n += 2;

    /* finally copy over to the buffer */
    strncpy( buf, tmp_msg, *n );

    pthread_mutex_unlock( lock );
    sem_post( mutex );
}

/**
 * @brief get current device's state.
 *
 * @param dv_name the device name of interest.
 * @param buf the buffer for the client, to make a tailor made response. In
 * other words, THIS GETS MODIFIED.
 * @param n the buffer length var. this also gets modified when buf gets
 * modifed.
 *
 * @note Returns 1 for no such device error, returns 0 otherwise.
 */
static int get_dev_state( const char *dv_name, char *buf, int *n )
{
    int rv = 1; /* return value */
    int loc = -1; /* location of a device match */

    sem_wait( mutex );
    pthread_mutex_lock( lock );

    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        if ( strncasecmp(memory[i].dev_name, dv_name, strlen(dv_name)) == 0 )
        {
            rv = 0;
            loc = i;
            break;
        }
    }

    if ( !rv )
    {
        /* create new temporary buffers */
        char tmp[DV_STATE_LEN];
        char tmp_cmnds[DB_CMND_LEN];
        char elem[JSON_LEN];

        /* memset these buffers to prevent problems */
        memset( tmp, 0, DV_STATE_LEN );
        memset( tmp_cmnds, 0, DB_CMND_LEN );
        memset( elem, 0, JSON_LEN );

        /* copy valid commands, ready to go. */
        strncpy( tmp_cmnds, memory[loc].valid_cmnds,
                 strlen(memory[loc].valid_cmnds) );

        char *tok;
        tok = strtok(tmp_cmnds, ",");

        /* start with the desired message */
        strncat( tmp, MESSAGE_206, MESSAGE_206_LEN );
        strncat( tmp, ":\n", 3 );

        while ( tok != 0 )
        {
            strncat( tmp, tok, strlen(tok) );
            strncat( tmp, " : ", 4 );
            find_jsmn_str( elem, tok, memory[loc].dev_state );
            strncat( tmp, elem, strlen(elem) );
            strncat( tmp, "\n", 2 );

            /* next on the list */
            tok = strtok( 0, "," );
        }

        /* create a terminating character for this. */
        strncat( tmp, ".\n", 3 );

        /* Create response */
        int len = strlen(memory[loc].dev_name) + strlen(tmp) + 1;
        *n = snprintf( buf, len, tmp, KL_VERSION, memory[loc].dev_name );
    }

    pthread_mutex_unlock( lock );
    sem_post( mutex );

    return rv;
}

/**
 * @brief Create, Initialize, and return the server's socketfd.
 *
 * @param port the port the server listens on.
 *
 * @note Returns -1 if an error occurs, the actual integer otherwise.
 */
int create_server_socket( const int port )
{
    int fd;
    struct sockaddr_in serv_addr;

    /* actually create the socket here. */
    fd = socket( AF_INET, SOCK_STREAM, 0 );

    if ( fd < 0 )
    {
#ifdef DEBUG
        perror( "error creating socket" );
        log_error( "error creating socket" );
#endif

        return -1;
    }

    /* set serv_addr to 0 initially. */
    memset( &serv_addr, 0, sizeof(serv_addr) );

    serv_addr.sin_family = AF_INET;

    /* allow anyone and anything to connect .. for now. */
    serv_addr.sin_addr.s_addr = htonl( INADDR_ANY );
    //inet_pton( AF_INET, IP_ADDR, &serv_addr.sin_addr );

#ifdef DEBUG
    log_debug( "using port %u for server", (port & 0xffff) );
#endif

    serv_addr.sin_port = htons( port & 0xffff );

    /* Bind the fd */
    if ( bind( fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr) ) < 0 )
    {
#ifdef DEBUG
        perror( "Error binding" );
        log_error( "Error binding server socket" );
#endif

        fd = -1;
    }

    /* Return the completed process. */
    return fd;
}

/**
 * @brief the server's connection handler
 *
 * @param connfds the pollfd array of clients.
 * @param num the count of clients in the pollfd array.
 *
 */
static void server_connection_handler( struct pollfd *connfds, const int num )
{
    int count; /* To handle connections */
    int n = 0; /* get the length of read() and write() functions */
    int response_len = 0; /* the server's response length to client */
    int status = 0; /* Status according to what server client wants */

    for ( count = 1; count <= num; count++ )
    {
        /* No connection, move on */
        if ( connfds[count].fd < 0 )
        {
            continue;
        }

        if ( connfds[count].revents & POLLIN )
        {
            /* Retreive request from client */
            n = read( connfds[count].fd, server_buffer[count - 1],
                      conf->buffer_size );

            /* client must have disconnected, move on */
            /* set equal to 0 if bugs come up! */
            if ( n <= 0 )
            {
                close( connfds[count].fd );
                connfds[count].fd = -1;
                continue;
            }

            /* Parse incoming request */
            status = parse_server_request( server_buffer[count - 1],
                                           &response_len );

            /* Write response to client */
            n = write( connfds[count].fd, server_buffer[count - 1],
                       response_len );

            /* Reset respective buffer */
            memset( server_buffer[count - 1], 0, conf->buffer_size );

            /* Handle exit if client wants to exit */
            if ( status < 0 )
            {
                close( connfds[count].fd );
                connfds[count].fd = -1;
            }
        }
    }
}

/**
 * @brief the server's network loop
 *
 * @param listenfd the socket created using
 * the function create_server_socket(port)
 *
 * @note Returns 1 for any errors that
 * may arise, returns 0 otherwise.
 */
int server_loop( const int listenfd )
{
    int rv = 0; /* return value, assume all is well */
    int connfd; /* the client's fd if one is accepted */
    int maxi = 0;
    int nready;
    int count;
    struct sockaddr_in cli_addr;
    socklen_t cli_addr_len = sizeof(cli_addr);

    /* first client is the server itself. */
    clientfds[0].fd = listenfd;
    clientfds[0].events = POLLIN;

    /* Initialize all other clients. */
    for ( int i = 1; i < POLL_SIZE; i++ )
    {
        clientfds[i].fd = -1;
    }

    /* run forever! */
    for ( ;; )
    {
        /* Wait for file descriptor to be ready in 50ms */
        nready = poll( clientfds, maxi+1, 50 );

        if ( nready < 0 )
        {
#ifdef DEBUG
            log_error( "Error polling in server" );
#endif

            rv = 1;
            break;
        }

        if ( clientfds[0].revents & POLLIN )
        {
            /* Accept some clients! */
            if ( (connfd = accept(listenfd, (struct sockaddr*)&cli_addr,
                                  &cli_addr_len)) < 0 )
            {
                /*
                 * Handle potential interrupt beyond our control,
                 * and just start over again.
                 */
                if ( errno == EINTR )
                {
#ifdef DEBUG
                    log_info( "Got interrupted with EINTR, continuing." );
#endif

                    continue;
                }
                else
                {
#ifdef DEBUG
                    log_error( "Error Accepting client in server" );
#endif
                    rv = 1;
                    break;
                }
            }

#ifdef DEBUG
            log_info( "Accepted new client %s:%d",
                      inet_ntoa(cli_addr.sin_addr),
                      cli_addr.sin_port );
#endif

            /* Add the client to next available clientfds slot. */
            for ( count = 1; count < POLL_SIZE; count++ )
            {
                if ( clientfds[count].fd < 0 )
                {
                    clientfds[count].fd = connfd;
                    break;
                }

                /*
                 * Made it this far into the loop,
                 * increment count, there are too many clients.
                 * Handle accordingly.
                 */
                if ( count == (POLL_SIZE - 1) )
                {
                    count++;
                }
            }

            /*
             * If there are too many clients, tell the
             * interested party that there are too many
             * clients right now.
             */
            if ( count >= POLL_SIZE )
            {
#ifdef DEBUG
                log_warn( "Too many clients reached!" );
#endif

                write( connfd, MESSAGE_505, MESSAGE_505_LEN );
                close( connfd );
            }

            /* Set the clientfds as poll input. */
            clientfds[count].events = POLLIN;

            /* set or reset maxi if count is greater than it. */
            maxi = ( count > maxi ? count : maxi );

            if ( --nready <= 0 )
            {
                continue;
            }
        }

        /* handle the connection */
        server_connection_handler( clientfds, maxi );

        /* If it's exit time, exit cleanly */
        if ( closeSocket > 0 )
        {
            rv = 0;
            close( clientfds[0].fd );
            clientfds[0].fd = -1;
            break;
        }
    }

    return rv;
}

/* ability to close the socket */
void close_socket()
{
    closeSocket = 1;
}

/*******************************************************************************
 * Everything related to mqtt will reside here.
 ******************************************************************************/

/**
 * @brief Create a non-blocking socket for mqtt use.
 *
 * @param addr The address of an mqtt broker server.
 * @param port The port the mqtt broker server listens on.
 *
 * @note Returns -1 if an error occurs, the actual integer otherwise.
 */
int open_nb_socket( const char *addr, const char *port )
{
    struct addrinfo hints = {0};

    hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Must be TCP */
    int sockfd = -1;
    int rv;
    struct addrinfo *p, *servinfo;

#ifdef DEBUG
    log_debug( "using port %s for mqtt", port );
#endif

    /* get address information */
    rv = getaddrinfo(addr, port, &hints, &servinfo);
    if ( rv != 0 )
    {
#ifdef DEBUG
        fprintf(stderr, "Failed to open socket (getaddrinfo): %s\n",
        gai_strerror(rv));
        log_error( "Failed to open socket (getaddrinfo): %s\n",
        gai_strerror(rv) );
#endif

        return -1;
    }

    /* open the first possible socket */
    for ( p = servinfo; p != NULL; p = p->ai_next )
    {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if ( sockfd == -1 )
        {
            continue;
        }

        /* connect to server */
        rv = connect(sockfd, p->ai_addr, p->ai_addrlen);
        if ( rv == -1 )
        {
          close(sockfd);
          sockfd = -1;
          continue;
        }
        break;
    }

    /* free servinfo */
    freeaddrinfo(servinfo);

    /* make non-blocking */
    if ( sockfd != -1 )
    {
        fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
    }
    /* return the new socket fd */
    return sockfd;
}

/**
 * @brief The Mqtt init function.
 *
 * @param client the mqtt_client struct.
 * @param sockfd the mqtt server's sockfd.
 * @param snd_buf the mqtt client's send buffer.
 * @param recv_buf the mqtt client's receive buffer.
 * @param conf the configuration struct.
 *
 * @note returns 1 for when an error occurs, 0 otherwise.
 */
int initialize_mqtt( struct mqtt_client *client, int *sockfd,
                               uint8_t *snd_buf, uint8_t *recv_buf,
                               config *conf )
{
    mqtt_init( client, *sockfd, snd_buf, conf->snd_buff,
               recv_buf, conf->recv_buff, publish_kl_callback );

    /* Create an anonymous session */
    const char* client_id = NULL;

    /* Ensure we have a clean session */
    uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;

    /* Send connection request to the broker. */
    mqtt_connect( client, client_id, NULL, NULL, 0, NULL, NULL,
    connect_flags, KEEP_ALIVE );

    /* check that we don't have any errors */
    if (client->error != MQTT_OK)
    {
#ifdef DEBUG
        log_error( "mqtt error: %s", mqtt_error_str(client->error) );
#endif

        return 1;
    }

    cl = client;

    return 0;
}

/**
 * @brief The mqtt publish (when subscribed) callback function.
 *
 * @param client not used
 * @param published contains topic_name and application_message
 *
 */
void publish_kl_callback( void** client,
                         struct mqtt_response_publish *published )
{
    /*
     * If a device state changes, update it accordingly
     */
    sem_wait( mutex );
    pthread_mutex_lock( lock );

    int topic_found = -1;
    int loc = -1;

    strncpy( app_msg, published->application_message,
             published->application_message_size );

    // for debugging purposes
#ifdef DEBUG
    printf( "%s\n", published->topic_name );
    printf( "app_msg: %s\n", app_msg );
#endif

    /* Find a match! */
    for ( int i = 0; i < conf->max_dev_count; i++ )
    {
        prepare_topic( STAT, memory[i].mqtt_topic, (char *)RESULT );
        if ( strncasecmp(topic, published->topic_name,
                         strlen(topic)) == 0 )
        {
#ifdef DEBUG
            printf( "found match\n" );
#endif
            topic_found = 1;
            loc = i;
            break;
        }

        /* no matching topics? for now just quietly ignore. */
        if ( i == (conf->max_dev_count - 1) )
        {
            topic_found = 0;
        }
    }

    /* match found, update the dev_state */
    if ( topic_found )
    {
        /* Check if app message is the full state */
        if ( published->application_message_size < DV_STATE_TMPL_LEN )
        {
            /* change only the locations of interest */
            replace_jsmn_property( memory[loc].dev_state, app_msg );
        }
        else
        {
            /* memset the dev_state */
            memset( memory[loc].dev_state, 0, DV_STATE_LEN );

            /* copy app_msg as state */
            strncpy( memory[loc].dev_state, app_msg,
                     published->application_message_size );
        }

        /* make sure a change is not already staged */
        switch( to_change[loc] )
        {
            // Only state needs to be updated, continue
            case -1:
            case 0:
                to_change[loc] = 0;
                break;

            // dev_name is to be updated, stage as a full update
            case 1:
            {
                strncpy( memory[loc].omqtt_topic,
                         memory[loc].mqtt_topic,
                         strlen(memory[loc].mqtt_topic) );

                to_change[loc] = 3;
                break;
            }

            // mqtt_topic is to be updated, stage a full update
            case 2:
            {
                strncpy( memory[loc].odev_name,
                         memory[loc].dev_name,
                         strlen(memory[loc].dev_name) );

                to_change[loc] = 3;
                break;
            }

            default:
            {
                /* ignore, leave value alone */
#ifdef DEBUG
                log_warn( "Found case %d", to_change[loc] );
#endif
                break;
            }
        }
    }

    /* memset topic and app message buffers for later use */
    memset( topic, 0, conf->topic_buff );
    memset( app_msg, 0, conf->app_msg_buff );

    /* All done! (for now) */
    pthread_mutex_unlock( lock );
    sem_post( mutex );
}

/**
 * @brief the refresher which sends/receives mqtt responses
 * at every roughly 100ms
 *
 * @param client is the struct mqtt_client to pass in.
 */
void *client_refresher(void* client)
{
    while(1)
    {
        mqtt_sync( (struct mqtt_client*) client );

        usleep( 100000U );
    }
    return NULL;
}
