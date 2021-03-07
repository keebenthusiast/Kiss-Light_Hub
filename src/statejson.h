/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019-2021, Christian Kissinger
 * kiss-light Hub is released under the New BSD license (see LICENSE).
 * Go to the project repo here:
 * https://gitlab.com/kiss-light-project/Kiss-Light_Hub
 *
 * Written by: Christian Kissinger
 */

#ifndef STATEJSON_H_
#define STATEJSON_H_

/* Constants */

enum {
    TOK_LEN = 64,
    JSON_LEN = 512
};

/* prototypes */
int find_jsmn_str( char *dst, const char *property, const char *state );
int replace_jsmn_property( char *state, const char *nstate );

#endif