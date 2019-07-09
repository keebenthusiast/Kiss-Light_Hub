
// system-related includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

// local includes
#include "daemon.h"
#include "common.h"
#include "log.h"

// local pid file descriptor
static int pid_fd = -1;

/*  
 * Brief Callback function for handling signals.
 * Param    sig     identifier of signal
 */
void handle_signal( int sig )
{
    /* Stop the daemon... cleanly. */
    if ( sig == SIGINT ) 
    {
        write_to_log( (char *)"stopping daemon" );
        stop_conf_parser();

        /* Unlock and close the lockfile. */
        if ( pid_fd != -1 )
        {
            lockf( pid_fd, F_ULOCK, 0 );
            close( pid_fd );
        }

        /* Delete lockfile. */
        if ( PIDFILE != NULL )
        {
            unlink( PIDFILE );
        }

        /* Reset signal handling to default behavior */
        signal( SIGINT, SIG_DFL );
    }
    /* Ignore a given SIGCHLD. */
    else if ( sig == SIGCHLD )
    {
        write_to_log( (char *)"Debug: recieved SIGCHLD signal" );
    }
    /* Ignore anything else too. */
    else
    {
        write_to_log( (char *)"Debug: recieved Unknown signal" );
    }
    
}

/*
 * This function will daemonize kiss-light.
 */
static void daemonize()
{
    write_to_log( (char *)"daemonizing" );
    pid_t pid = 0;
    int fd;

    /* Fork from parent process. */
    pid = fork();

    /* Error forking occurred. */
    if ( pid < 0 )
    {
        write_to_log( (char *)"unable to fork from parent process" );
        exit( EXIT_FAILURE );
    }

    /* Though if successful, terminate the parent process. */
    if ( pid > 0 )
    {
        exit( EXIT_SUCCESS );
    }

    /* Set the child process to become the new session leader. */
    if ( setsid() < 0 )
    {
        write_to_log( (char *)"unable to set child process to become session leader" );
        exit( EXIT_FAILURE );
    }
    
    /* Ignore signal sent from child to parent process */
    signal( SIGCHLD, SIG_IGN );

    /* 
     * This seems to be a tester to make sure the SIGCHLD is indeed ignored.
     */
    /* Fork again.. */
    pid = fork();

    /* Error forking occurred.. */
    if ( pid < 0 )
    {
        write_to_log( (char *)"unable to fork the second time" );
        exit( EXIT_FAILURE );
    }

    /* Though if successful, terminate the parent process. */
    if ( pid > 0 )
    {
        exit( EXIT_SUCCESS );
    }

    /* Set new file permissions. */
    umask( 0 );

    /* Change the working directory to another dir */
    if ( (chdir( "/" )) < 0 )
    {
        write_to_log( (char *)"unable to chdir to '/'" );
        exit( EXIT_FAILURE );
    }

    /* Close file descriptors, then reopen them to '/dev/null' */
    for ( fd = sysconf(_SC_OPEN_MAX); fd > 0; fd-- )
    {
        close( fd );
    }

    stdin = fopen( "/dev/null", "r" );
    stdout = fopen( "/dev/null", "w+" );
    stderr = fopen( "/dev/null", "w+" );

    /* Now to write PID of daemon to Lockfile, then done. */

    if ( PIDFILE != NULL )
    {
        char str[256];
        pid_fd = open( PIDFILE, (O_RDWR | O_CREAT), 0640 );

        if ( pid_fd < 0 )
        {
            /* Cannot open lockfile. */
            write_to_log( (char *)"unable to open lockfile" );
            exit( EXIT_FAILURE );
        }

        if ( lockf(pid_fd, F_TLOCK, 0) < 0 )
        {
            /* Cannot lock lockfile. */
            write_to_log( (char *)"unable to lock the lockfile" );
            exit( EXIT_FAILURE );
        }

        /* Get the current PID */
        sprintf( str, "%d\n", getpid() );

        /* Write PID to Lockfile */
        write( pid_fd, str, strlen(str) );
    }
}

/* 
 * Function for main function in server.cpp to allow
 * running as a daemon.
 */
void run_as_daemon()
{
    daemonize();
}
