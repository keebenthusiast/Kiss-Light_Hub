/*
 * Strictly for configuration related information.
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
#include <unistd.h>
#include <signal.h>

// local includes
#include "config.h"
#include "ini.h"

#ifdef DEBUG
#include "log.h"
#endif

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
 * and implementation on how it works.
 */
static int ini_callback_handler( void* user, const char* section,
                                 const char* name, const char* value )
{
    config *pconfig = (config *)user;

    /* a quick little shortcut that utilizes strncmp */
#define MATCH(s, sl, n, nl) strncmp(section, s, sl) == 0 && \
    strncmp(name, n, nl) == 0

    // Network
    if ( MATCH(NETWORK, NETWORK_LEN, PORT, PORT_LEN) )
    {
        pconfig->port = atoi( value );
    }
    else if ( MATCH(NETWORK, NETWORK_LEN, BUF_SIZE, BUF_SIZE_LEN) )
    {
        pconfig->buffer_size = atoi( value );
    }
    // Mqtt
    else if ( MATCH(MQTT, MQTT_LEN, MQTT_SRVR, MQTT_SRVR_LEN) )
    {
        pconfig->mqtt_server = strndup( value, strlen(value) );
    }
    else if ( MATCH(MQTT, MQTT_LEN, MQTT_PORT, MQTT_PORT_LEN) )
    {
        pconfig->mqtt_port = atoi( value );
    }
    else if ( MATCH(MQTT, MQTT_LEN, RECV_BUF, RECV_BUF_LEN) )
    {
        pconfig->recv_buff = atoi( value );
    }
    else if ( MATCH(MQTT, MQTT_LEN, SND_BUF, SND_BUF_LEN) )
    {
        pconfig->snd_buff = atoi( value );
    }
    else if ( MATCH(MQTT, MQTT_LEN, TPC_BUF, TPC_BUF_LEN) )
    {
        pconfig->topic_buff = atoi( value );
    }
    else if ( MATCH(MQTT, MQTT_LEN, MSG_BUF, MSG_BUF_LEN) )
    {
        pconfig->app_msg_buff = atoi( value );
    }
    // Database
    else if ( MATCH(DATABASE, DATABASE_LEN, DB_LOC, DB_LOC_LEN) )
    {
        pconfig->db_loc = strndup( value, strlen(value) );
    }
    else if ( MATCH(DATABASE, DATABASE_LEN, DB_BUFF, DB_BUF_LEN) )
    {
        pconfig->db_buff = atoi( value );
    }
    else if ( MATCH(DATABASE, DATABASE_LEN, MAX_DEV_COUNT, MAX_DEV_COUNT_LEN) )
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
    if ( ini_parse( CONF_LOCATION, ini_callback_handler, cfg) < 0 )
    {

#ifdef DEBUG
        log_error( "Cannot load config file '%s'", CONF_LOCATION );
#endif

        return 1;
    }

    return 0;
}
