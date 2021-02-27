/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019-2021, Christian Kissinger
 * kiss-light Hub is released under the New BSD license (see LICENSE).
 * Go to the project repo here:
 * https://gitlab.com/kiss-light-project/Kiss-Light_Hub
 *
 * Written by: Christian Kissinger
 */

#ifndef CONFIG_H_
#define CONFIG_H_

/* Useful Constants */
#define CONF_LOCATION ((const char *)"/etc/kisslight.ini")

// sections
#define NETWORK       ((const char *)"network")
#define MQTT          ((const char *)"mqtt")
#define DATABASE      ((const char *)"database")

// names
#define PORT          ((const char *)"port")
#define BUF_SIZE      ((const char *)"buffer_size")
#define MQTT_SRVR     ((const char *)"mqtt_server")
#define MQTT_PORT     ((const char *)"mqtt_port")
#define RECV_BUF      ((const char *)"recv_buff")
#define SND_BUF       ((const char *)"snd_buff")
#define TPC_BUF       ((const char *)"topic_buff")
#define MSG_BUF       ((const char *)"app_msg_buff")
#define DB_LOC        ((const char *)"db_location")
#define DB_BUFF       ((const char *)"db_buff")
#define MAX_DEV_COUNT ((const char *)"max_dev_count")

enum {

    // section lens
    NETWORK_LEN = 8,
    MQTT_LEN = 5,
    DATABASE_LEN = 9,

    // name lens
    PORT_LEN = 5,
    BUF_SIZE_LEN = 12,
    MQTT_SRVR_LEN = 12,
    MQTT_PORT_LEN = 10,
    RECV_BUF_LEN = 10,
    SND_BUF_LEN = 9,
    TPC_BUF_LEN = 11,
    MSG_BUF_LEN = 13,
    DB_LOC_LEN = 12,
    DB_BUF_LEN = 8,
    MAX_DEV_COUNT_LEN = 14

};

/* prototypes */
int initialize_conf_parser();

/**
 * @typedef config
 * @brief configuration parser struct
 */
typedef struct
{
    int port;
    int buffer_size;
    const char *mqtt_server;
    int mqtt_port;
    int recv_buff;
    int snd_buff;
    int topic_buff;
    int app_msg_buff;
    const char *db_loc;
    int db_buff;
    int max_dev_count;
} config;

#endif
