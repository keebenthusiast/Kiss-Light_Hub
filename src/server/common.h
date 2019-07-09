#ifndef COMMON_H_
#define COMMON_H_

#define CONFLOCATION ((const char*)"/etc/kiss-light.ini")

/* misc, mostly for logging */
void get_current_time( char *buf );

/* 
 * network-related, actual protocol
 * resides here.
 */
int parse_input( char *buf, int *n );

/* electrical related */
void initialize_rc_switch();
void stop_rc_switch();
void send_rf_signal( int code, int pulse );
void sniff_rf_signal( int &code, int &pulse );


/* configuration parser related */
void initialize_conf_parser();
void stop_conf_parser();
long get_int( const char *section, const char *name, long def_val );

#endif