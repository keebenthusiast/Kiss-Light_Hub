/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019-2021, Christian Kissinger
 * kiss-light Hub is released under the New BSD license (see LICENSE).
 * Go to the project repo here:
 * https://gitlab.com/kiss-light-project/Kiss-Light_Hub
 *
 * Written by: Christian Kissinger
 */

#ifndef DATABASE_H_
#define DATABASE_H_

/* Includes in case the compiler complains */
#include <pthread.h>
#include <semaphore.h>
#include "sqlite3/sqlite3.h"
#include "config.h"

/* Useful constants regarding database reside here */

// dev types
#define DEV_TYPE0 ((const char *)"outlet/toggleable")
#define DEV_TYPE1 ((const char *)"powerstrip")
#define DEV_TYPE2 ((const char *)"dimmablebulb")
#define DEV_TYPE3 ((const char *)"cctbulb")
#define DEV_TYPE4 ((const char *)"rgbbulb")
#define DEV_TYPE5 ((const char *)"rgbwbulb")
#define DEV_TYPE6 ((const char *)"rgbcctbulb")
#define DEV_TYPE7 ((const char *)"custom")

// valid commands per dev type
#define DEV_TYPE0_CMDS ((const char *)"POWER")
#define DEV_TYPE1A_CMD ((const char *)"POWER%d,")
#define DEV_TYPE1B_CMD ((const char *)"POWER%d")
#define DEV_TYPE2_CMDS ((const char *)"POWER,DIMMER")
#define DEV_TYPE3_CMDS ((const char *)"POWER,DIMMER,COLOR,WHITE,CT")
#define DEV_TYPE4_CMDS ((const char *)"POWER,DIMMER,COLOR,HSBCOLOR")
#define DEV_TYPE5_CMDS ((const char *)"POWER,DIMMER,COLOR,HSBCOLOR,WHITE")
#define DEV_TYPE6_CMDS ((const char *)"POWER,DIMMER,COLOR,HSBCOLOR,WHITE,CT")

// placeholder dev state
#define DEV_STATE_TMPL ((const char *)"{\"Time\":\"UNKNOWN\",\"Uptime\":" \
"\"UNKNOWN\",\"UptimeSec\":0,\"Heap\":0,\"SleepMode\":\"UNKNOWN\",\"Sleep" \
"\":0,\"LoadAvg\":0,\"MqttCount\":0,\"POWER\":\"UKNOWN\",\"Wifi\":{\"AP\":0," \
"\"SSId\":\"UNKNOWN\",\"BSSId\":\"00:00:00:00:00:00\",\"Channel\":0,\"RSSI" \
"\":0,\"Signal\":-1,\"LinkCount\":0,\"Downtime\":\"UNKNOWN\"}}")

// column names
#define DEV_NAME  ((const char *)"dev_name")
#define MQTT_TPC  ((const char *)"mqtt_topic")
#define DEV_TYPE  ((const char *)"dev_type")
#define DEV_STATE ((const char *)"dev_state")
#define VLD_CMDS  ((const char *)"valid_cmnds")
#define COUNT     ((const char *)"COUNT(*)")

// Queries
#define GET_LEN_QUERY ((const char *)"SELECT COUNT(*) FROM device;")
#define DB_DUMP_QUERY ((const char *)"SELECT dev_name, mqtt_topic, " \
"dev_type, dev_state, valid_cmnds FROM device;")
#define INSERT_QUERY  ((const char *)"INSERT INTO device VALUES('%s'," \
" '%s', %d, '%s', '%s');")
#define DELETE_QUERY  ((const char *)"DELETE FROM device WHERE dev_name" \
"='%s' AND mqtt_topic='%s';")

/* Update queries */
#define STATE_QUERY   ((const char *)"UPDATE device SET dev_state='%s' WHERE"\
" dev_name='%s' AND mqtt_topic='%s';")
#define NAME_QUERY    ((const char *)"UPDATE device SET dev_name='%s' WHERE"\
" dev_name='%s' AND mqtt_topic='%s';")
#define MQTT_QUERY    ((const char *)"UPDATE device SET mqtt_topic='%s' "\
"WHERE dev_name='%s' AND mqtt_topic='%s';")

enum {
    DB_DATA_LEN = 64,
    DB_CMND_LEN = 256,

    /* For the device id
     * to string function.
     *
     * Also used in main.
     */
    DEV_TYPE_LEN = 64,

    DV_STATE_LEN = 1024,

    // dev type lens
    DEV_TYPE0_LEN = 18,
    DEV_TYPE1_LEN = 11,
    DEV_TYPE2_LEN = 13,
    DEV_TYPE3_LEN = 8,
    DEV_TYPE4_LEN = 8,
    DEV_TYPE5_LEN = 9,
    DEV_TYPE6_LEN = 11,
    DEV_TYPE7_LEN = 7,

    // dev type valid commands
    DEV_TYPE0_CMDS_LEN = 6,
    DEV_TYPE1A_CMD_LEN = 8,
    DEV_TYPE1B_CMD_LEN = 7,
    DEV_TYPE2_CMDS_LEN = 13,
    DEV_TYPE3_CMDS_LEN = 28,
    DEV_TYPE4_CMDS_LEN = 28,
    DEV_TYPE5_CMDS_LEN = 34,
    DEV_TYPE6_CMDS_LEN = 37,

    // for checking purposes.
    DEV_TYPE_MAX = 7,

    // Dev state template len
    DV_STATE_TMPL_LEN = 265,

    // column name lens
    DEV_NAME_LEN = 9,
    MQTT_TPC_LEN = 11,
    DV_TYPE_LEN = 9,
    DEV_STATE_LEN = 10,
    VLD_CMDS_LEN = 12,
    COUNT_LEN = 9,

    // query lens
    /* NOTE:
     * Placeholder parts of strings are
     * not included in length,
     * hence the differences in length
     */
    GET_LEN_QUERY_LEN = 29,
    DB_DUMP_QUERY_LEN = 75,
    INSERT_QUERY_LEN = 45,
    DELETE_QUERY_LEN = 56,
    STATE_QUERY_LEN = 68,
    NAME_QUERY_LEN = 67,
    MQTT_QUERY_LEN = 69,

    // Seconds to sleep
    SLEEP_DELAY = 5U,

};

/**
 * @typedef db_data
 * @brief database output struct
 */
typedef struct
{
    char dev_name[DB_DATA_LEN];
    char mqtt_topic[DB_DATA_LEN];
    int dev_type;
    char dev_state[DV_STATE_LEN];
    char valid_cmnds[DB_CMND_LEN];

    /*
     * for database usage,
     * for old entries.
     */
    char odev_name[DB_DATA_LEN];
    char omqtt_topic[DB_DATA_LEN];

} db_data;


/* Prototypes for various functions */
int initialize_db( config *cfg, sqlite3 *db, char *sql_buffer, db_data *dat,
                   int *to_chng, char *dv_str, pthread_mutex_t *lck,
                   sem_t *mtx );

const int get_current_entry_count();
void decrement_db_count();
void increment_db_count();
int check_device_type( const int in );
char *device_type_to_str( const int in );
int get_digit_count( const int in );
void powerstrip_cmnd_cat( char *dst, const int count );

void *db_updater( void* args );

#endif
