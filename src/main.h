/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019-2021, Christian Kissinger
 * kiss-light Hub is released under the New BSD license (see LICENSE).
 * Go to the project repo here:
 * https://gitlab.com/kiss-light-project/Kiss-Light_Hub
 *
 * Written by: Christian Kissinger
 */
#ifndef MAIN_H_
#define MAIN_H_

/* Includes */
#include "sqlite3/sqlite3.h"

/* Constants */
#define LOGLOCATION ((const char*)"/var/log/kisslight/kisslight.log")
#define DEBUG_LEVEL -1

enum {
    /* These MUST be exact powers of 2 */
    SQLITE_BUFFER_LEN = 262144,
    SQLITE_BUFFER_MIN = 2048,
};

#endif
