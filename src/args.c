/*
 * Where command line arguments are processed.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019-2021, Christian Kissinger
 * kiss-light Hub is released under the New BSD license (see LICENSE).
 * Go to the project repo here:
 * https://gitlab.com/kiss-light-project/Kiss-Light_Hub
 *
 * Written by: Christian Kissinger,
 */

// system-related includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

// local includes
#include "args.h"
#include "config.h"
#include "daemon.h"
#include "inih/ini.h"

#ifdef DEBUG
#include "log/log.h"
#endif

// pointer to config cfg;
//static config *conf;

/*******************************************************************************
 * Everything related to arg processing will reside here
 ******************************************************************************/

/**
 * TODO: This should be more fleshed out,
 * maybe a student can help with this part.
 *
 * @brief Process arguments passed in (from main).
 *
 * @param argc passing in argc from main would be easiest.
 * @param argv passing in argv from main would be easiest.
 *
 * @note Returns 0 for success, nonzero for error.
 */
int process_args( int argc, char **argv )
{
    int rv = 0;

    if ( argc >= 2 )
    {
        if ( strncmp( argv[1], "daemon", 7) == 0 )
        {
            rv = run_as_daemon();
        }
    }

    return rv;
}
