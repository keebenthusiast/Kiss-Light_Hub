#ifndef COMMON_H_
#define COMMON_H_

void get_current_time( char *buf );

int parse_input( char *buf, int *n );

void send_rf_signal( int code, int pulse );

void sniff_rf_signal( int &code, int &pulse );

#endif