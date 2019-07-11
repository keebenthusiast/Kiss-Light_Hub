#ifndef COMMON_H_
#define COMMON_H_

#define CONFLOCATION ((const char*)"/etc/kisslight.ini")

/* misc, mostly for logging */
void get_current_time( char *buf );

/* 
 * network-related, actual protocol
 * resides here.
 */
int parse_input( char *buf, int *n );

/* electrical related */
void initialize_rc_switch();
void send_rf_signal( int code, int pulse );
void sniff_rf_signal( int &code, int &pulse );


/* configuration parser related */
void initialize_conf_parser();
long get_int( const char *section, const char *name, long def_val );
const char *get_string( const char *section, const char *name, const char *def_val );

#endif