#ifndef DAEMON_H_
#define DAEMON_H_

#define PIDFILE ((const char*)"/run/lock/kisslight.pid")

void handle_signal( int sig );

static void daemonize();

#endif