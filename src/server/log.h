#ifndef LOG_H_
#define LOG_H_

#define LOGLOCATION ((const char*)"/var/log/kisslight.log")

void initialize_logger();
void write_to_log( char *str );

#endif