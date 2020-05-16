#ifndef COMMON_H_
#define COMMON_H_

#define CONFLOCATION ((const char *)"/etc/kisslight.ini")
#define DBLOCATION ((const char *)"/var/lib/kisslight/kisslight.db")
#define MAX_DEVICES 30
#define KL_VERSION 0.2
#define TIME_OUT 7

/* misc, mostly for logging */
void get_current_time( char *buf );

/* 
 * network related, actual protocol
 * resides here.
 */
int parse_server_input( char *buf, int *n );

/* configuration parser related */
void initialize_conf_parser();
long get_int( const char *section, const char *name, long def_val );
const char *get_string( const char *section, const char *name, const char *def_val );

#endif