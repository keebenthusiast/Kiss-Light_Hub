/*
 *
 * Written by: Christian Kissinger
 */

#ifndef DISCOVERY_H_
#define DISCOVERY_H_

/* Constants */
#define ADV_IP_ADDR ((const char*)"239.255.255.250")
#define SERVICE_NAME ((const char*)"kiss-light")
#define UUID_LOCATION ((const char*)"/etc/kisslight/uuid")

enum {
    // Default port for SSDP
    DISCOVERY_PORT = 1900

    // if more is needed later
};

/* To demo the program itself. */
//#define STANDALONE

/* Function prototypes */
void *discovery_handler( void *varg );

#endif