/*
 * The Daemonizer, borrowed from here:
 * https://github.com/jirihnidek/daemon
 *
 * Adapted by: Christian Kissinger
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019-2021, Christian Kissinger
 * kiss-light Hub is released under the New BSD license (see LICENSE).
 * Go to the project repo here:
 * https://gitlab.com/kiss-light-project/Kiss-Light_Hub
 *
 */

// system-related includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

// local includes
#include "server.h"
#include "daemon.h"
#include "common.h"
#include "log.h"

// local pid file descriptor
static int pid_fd = -1;

// local isDaemon boolean
static int isDaemon = 0;

/**
 * @brief Callback function for handling signals.
 *
 * @param sig identifier of signal
 */
void handle_signal( int sig )
{
    /* Stop the daemon... cleanly. */
    if ( sig == SIGINT )
    {
        log_trace( "Stopping server" );

        if ( isDaemon )
        {
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
        }

        /* Close server's socket */
        close_socket();

        /* Reset signal handling to default behavior. */
        signal( SIGINT, SIG_DFL );
    }
    /* Ignore a given SIGCHLD. */
    else if ( sig == SIGCHLD )
    {
        log_debug( "received SIGCHLD signal, ignored" );
    }
    /* Ignore a given SIGHUP */
    else if ( sig == SIGHUP )
    {
        log_debug( "received SIGHUP signal, ignored" );
    }
    /* Ignore anything else too. */
    else
    {
        log_debug( "Received signal not implemented to be handled: %d", sig );
    }

}

/**
 * @brief This function will daemonize kiss-light.
 *
 * @note It isn't called directly, use run_as_daemon() for that
 */
static int daemonize()
{
    log_trace( "Daemonizing" );
    pid_t pid = 0;
    int fd;

    /* Fork from parent process. */
    pid = fork();

    /* Error forking occurred. */
    if ( pid < 0 )
    {
        log_fatal( "Unable to fork from parent process" );
        return EXIT_FAILURE;
    }

    /* Though if successful, terminate the parent process. */
    if ( pid > 0 )
    {
        exit( EXIT_SUCCESS );
    }

    /* Set the child process to become the new session leader. */
    if ( setsid() < 0 )
    {
        log_fatal( "Unable to set child process to become session leader" );
        return EXIT_FAILURE;
    }

    /* Ignore signal sent from child to parent process */
    signal( SIGCHLD, SIG_IGN );
    signal( SIGHUP, SIG_IGN );

    /*
     * This seems to be a tester to make sure the SIGCHLD is indeed ignored.
     */
    /* Fork again.. */
    pid = fork();

    /* Error forking occurred.. */
    if ( pid < 0 )
    {
        log_fatal( "Unable to fork the second time" );
        return EXIT_FAILURE;
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
        log_fatal( "Unable to chdir to '/'" );
        return EXIT_FAILURE;
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
            log_fatal( "Unable to open lockfile" );
            return EXIT_FAILURE;
        }

        if ( lockf(pid_fd, F_TLOCK, 0) < 0 )
        {
            /* Cannot lock lockfile. */
            log_fatal( "Unable to lock the lockfile" );
            return EXIT_FAILURE;
        }

        /* Get the current PID */
        sprintf( str, "%d\n", getpid() );

        /* Write PID to Lockfile */
        write( pid_fd, str, strlen(str) );
    }

    return EXIT_SUCCESS;
}

/**
 * @brief Function to allow the program to run as a daemon.
 */
int run_as_daemon()
{
    int ret = daemonize();

    if ( !ret )
    {
        isDaemon = 1;
    }

    return ret;
}