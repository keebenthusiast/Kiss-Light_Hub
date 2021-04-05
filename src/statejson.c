/*
 * Change dev_states of devices given the input.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019-2021, Christian Kissinger
 * kiss-light Hub is released under the New BSD license (see LICENSE).
 * Go to the project repo here:
 * https://gitlab.com/kiss-light-project/Kiss-Light_Hub
 *
 * Written by: Christian Kissinger
 */

// system-related includes
#include <stdio.h>
#include <string.h>

// local includes
#include "statejson.h"
#include "jsmn/jsmn.h"

/**
 * @brief Simple function to do the strncmp work for the programmer.
 *
 * @param json the full json string of interest
 * @param tok the jsmn token to be passed in.
 * @param s the target property to compare against.
 *
 * @note Returns nonzero upon error.
 */
static int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
    int rv = -1;

    /*
     * If it MUST be a string, just include the following:
     *  tok->type == JSMN_STRING &&
     */
    if ( (int)strlen(s) == tok->end - tok->start &&
         strncasecmp(json + tok->start, s, tok->end - tok->start) == 0)
    {
        rv = 0;
    }

  return rv;
}

/**
 * @brief find the matching element, given given a device ID, property.
 *
 * @param dst Where the result gets stored, THIS GETS MODIFIED HERE!
 * @param property The desired member of a given state json string.
 * @param state the state json string.
 *
 * @note This may not be the most efficient means of retreiving
 * the desired element, but it will work as intended.
 * Return 0 upon success, nonzero for any error that occurs.
 */
int find_jsmn_str( char *dst, const char *property, const char *state )
{
    int rv = 0;
    jsmn_parser p;
    jsmntok_t t[TOK_LEN];

    jsmn_init( &p );
    int r = jsmn_parse(&p, state, strlen(state), t, TOK_LEN );

    for ( int i = 0; i < r; i++ )
    {
        if ( jsoneq(state, &t[i], property) == 0 )
        {
            snprintf(dst, t[i + 1].end - t[i + 1].start + 1, "%.*s",
                    t[i + 1].end - t[i + 1].start,
                    state + t[i + 1].start );

            break;
        }

        /* if nothing is found, set the return value to nonzero */
        if ( i == (r - 1) )
        {
            rv = 1;
        }
    }

    return rv;
}

/**
 * @brief Update parts of a state string given new state values
 *
 * @param state the state, THIS GETS MODIFIED HERE!
 * @param nstate the new state values.
 *
 * @note It is assumed that everything in the json is in order
 * with the state string, it may not work if everything is jumbled.
 * Returns 0 upon success, nonzero for any error that occurs.
 */
int replace_jsmn_property( char *state, const char *nstate )
{
    int rv = 0;
    jsmn_parser p;
    jsmn_parser pn;
    jsmntok_t t[TOK_LEN];
    jsmntok_t tn[TOK_LEN];
    char prop[JSON_LEN];
    char elem[JSON_LEN];
    char end[JSON_LEN];

    memset( prop, 0, JSON_LEN );
    memset( elem, 0, JSON_LEN );
    memset( end, 0, JSON_LEN );

    jsmn_init( &p );
    jsmn_init( &pn );

    int r = jsmn_parse( &p, state, strlen(state), t, TOK_LEN );
    int rn = jsmn_parse( &pn, nstate, strlen(nstate), tn, TOK_LEN );

    int i = 1;
    int j = 1;

    /* Copy first property and item to scan for */
    snprintf( prop, tn[j].end - tn[j].start + 1, "%.*s",
              tn[j].end - tn[j].start, nstate + tn[j].start );
    snprintf( elem, tn[j + 1].end - tn[j + 1].start + 1, "%.*s",
              tn[j + 1].end - tn[j + 1].start, nstate + tn[j + 1].start );

    for ( i = 1; i < r; i += 2 )
    {
        if ( jsoneq(state, &t[i], prop) == 0 )
        {
            /* if elements match, don't bother and move on */
            if ( jsoneq(state, &t[i + 1], elem) == 0 )
            {
                j += 2;

                if ( j >= rn )
                {
                    break;
                }

                snprintf( prop, tn[j].end - tn[j].start + 1, "%.*s",
                tn[j].end - tn[j].start, nstate + tn[j].start );
                snprintf( elem, tn[j + 1].end - tn[j + 1].start + 1, "%.*s",
                tn[j + 1].end - tn[j + 1].start, nstate + tn[j + 1].start );

                continue;
            }

            /* Update String here */
            int str_diff = strlen(elem) - (t[i + 1].end - t[i + 1].start);

            memcpy( end, state + t[i + 1].end, strlen(state) - t[i + 1].end );
            memset( state + t[i + 1].end, 0, strlen(state) - t[i + 1].end );
            memcpy( state + t[i + 1].start, elem, strlen(elem) );
            memcpy( state + t[i + 1].start + strlen(elem), end, strlen(end) );
            memset( end, 0, JSON_LEN );

            t[i + 1].end += str_diff;
            for ( int m = (i + 2); m < r; m++ )
            {
                t[m].start += str_diff;
                t[m].end += str_diff;
            }

            /* increment j and either finish or continue */
            j += 2;

            if ( j >= rn )
            {
                break;
            }

            snprintf( prop, tn[j].end - tn[j].start + 1, "%.*s",
            tn[j].end - tn[j].start, nstate + tn[j].start );
            snprintf( elem, tn[j + 1].end - tn[j + 1].start + 1, "%.*s",
            tn[j + 1].end - tn[j + 1].start, nstate + tn[j + 1].start );
        }
    }

    return rv;
}
