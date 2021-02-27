/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019-2021, Christian Kissinger
 * kiss-light Hub is released under the New BSD license (see LICENSE).
 * Go to the project repo here:
 * https://gitlab.com/kiss-light-project/Kiss-Light_Hub
 *
 * Written by: Christian Kissinger
 */

#ifndef DAEMON_H_
#define DAEMON_H_

#define PIDFILE ((const char*)"/run/lock/kisslight.pid")

/* prototypes */
void handle_signal( int sig );
int run_as_daemon();

#endif
