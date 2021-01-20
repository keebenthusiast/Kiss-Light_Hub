/*
 * The main file for this application,
 * so it can make things easier to follow.
 *
 * Written by: Christian Kissinger
 */

// system-related includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>
#include <sqlite3.h>

// local includes
#include "main.h"
#include "common.h"
#include "server.h"
#include "mqtt.h"
#include "log.h"

/*
 * A struct to keep track of various buffers
 */
typedef struct
{
    // Server buffers
    uint8_t **server_buffer;
    uint8_t **str_buffer;

    // SQL buffers
    char *sql_buffer;

    // Mqtt buffers
    uint8_t *send_buffer;
    uint8_t *receive_buffer;
    char *topic;
    char *application_message;

} buffers;

/*
 * Allocate buffers,
 * The buffers are memsetted
 * so valgrind will not complain.
 */
static void allocate_buffers( buffers *bfrs, config *cfg, db_data *data )
{
    log_trace( "allocating buffers" );
    log_debug( "allocating server buffers" );

    bfrs->server_buffer = malloc( (POLL_SIZE - 2) * sizeof(char *) );
    bfrs->str_buffer = malloc( ARG_LEN * sizeof(char *) );

    // One big loop to avoid the need for several
    for ( int i = 0; i < cfg->max_dev_count; i++ )
    {
        if ( i < ARG_LEN )
        {
            bfrs->str_buffer[i] = (uint8_t *)malloc(
                ARG_BUF_LEN * sizeof(uint8_t)
            );
            memset( bfrs->str_buffer[i], 0, ARG_BUF_LEN );
        }

        if ( i < (POLL_SIZE - 2) )
        {
            bfrs->server_buffer[i] =
            (uint8_t *)malloc( cfg->buffer_size * sizeof(uint8_t) );
            memset( bfrs->server_buffer[i], 0, cfg->buffer_size );
        }

        memset( data[i].dev_name, 0, DB_DATA_LEN );
        memset( data[i].mqtt_topic, 0, DB_DATA_LEN );
        data[i].dev_type = -1;
    }

    log_debug( "allocating sql buffer" );
    bfrs->sql_buffer = (char *)malloc( cfg->db_buff * sizeof(char) );
    memset( bfrs->sql_buffer, 0, cfg->db_buff );

    log_debug( "allocating mqtt buffers" );
    bfrs->send_buffer = (uint8_t *)malloc( cfg->snd_buff * sizeof(uint8_t) );
    memset( bfrs->send_buffer, 0, cfg->snd_buff );
    bfrs->receive_buffer = (uint8_t *)malloc(
        cfg->recv_buff * sizeof(uint8_t)
    );
    memset( bfrs->receive_buffer, 0, cfg->recv_buff );
    bfrs->topic = (char *)malloc( cfg->topic_buff * sizeof(char) );
    memset( bfrs->topic, 0, cfg->topic_buff );
    bfrs->application_message = (char *)malloc(
        cfg->app_msg_buff * sizeof(char)
    );
    memset( bfrs->application_message, 0, cfg->app_msg_buff );

    log_trace( "all buffers allocated" );
}

/*
 * A local clean-up function for when the program has to exit.
 */
static void cleanup( FILE *lg, buffers *bfrs, config *cfg, db_data *data,
                     struct mqtt_client *client )
{
    log_trace( "freeing allocated memory" );

    log_debug( "Freeing config data allocated" );
    /* Clean up allocated strings */
    if ( cfg->db_loc != NULL )
    {
        free( (void*)cfg->db_loc );
        cfg->db_loc = NULL;
    }

    if ( cfg->mqtt_server != NULL )
    {
        free( (void*)cfg->mqtt_server );
        cfg->mqtt_server = NULL;
    }

    /* Finally free allocated memory used by config */
    free( cfg );
    cfg = NULL;

    log_debug( "config data freed" );

    log_debug( "freeing buffers" );
    /* Now to free the memory allocated by buffers */
    for ( int i = 0; i < (POLL_SIZE - 2); i++ )
    {
        if ( i < ARG_LEN )
        {
            free( bfrs->str_buffer[i] );
            bfrs->str_buffer[i] = NULL;
        }

        free( bfrs->server_buffer[i] );
        bfrs->server_buffer[i] = NULL;
    }

    free( data );
    data = NULL;

    free( bfrs->server_buffer );
    bfrs->server_buffer = NULL;

    free( bfrs->str_buffer );
    bfrs->str_buffer = NULL;

    free( bfrs->sql_buffer );
    bfrs->sql_buffer = NULL;

    free( bfrs->send_buffer );
    bfrs->send_buffer = NULL;

    free( bfrs->receive_buffer );
    bfrs->receive_buffer = NULL;

    free( bfrs->topic );
    bfrs->topic = NULL;

    free( bfrs->application_message );
    bfrs->application_message = NULL;

    free( bfrs );
    bfrs = NULL;
    log_debug( "buffers freed" );

    if ( client != NULL )
    {
        log_debug( "Freeing mqtt_client struct" );
        free( client );
        client = NULL;
        log_debug( "mqtt_client struct freed" );
    }

    log_trace( "memory freed, closing logger" );

    /* Finally, close log file */
    fclose( lg );

}

/*
 * Where it all begins!
 */
int main( int argc, char **argv )
{
    /* Initialize logger */
    FILE *lg = fopen( LOGLOCATION, "a" );

    if ( lg == NULL )
    {
        fprintf( stderr, "Failed to open log file %s\n", LOGLOCATION );
        return 1;
    }

    log_add_fp(lg, DEBUG_LEVEL);
    log_trace( "Kiss-Light Logger initialized");

    /* Initialize configuration parser */
    config *cfg = (config *)malloc( sizeof(config) );

    if ( initialize_conf_parser(cfg) != 0 )
    {
        log_error( "Unable to initialize configuration parser, exiting..." );
        return 1;
    }
    log_trace( "Configuration parser initialized" );

    /* Analyze command line args (should override config if specified to) */
    /* This is also where running as a daemon would be specified */
    process_args( argc, argv );
    log_trace( "args processed" );

    /* Allocate Buffers for server, mqtt functions, sqlite functions */
    buffers *bfrs = (buffers *)malloc( sizeof(buffers) );
    db_data *data = (db_data *)malloc( cfg->max_dev_count * sizeof(db_data) );
    allocate_buffers( bfrs, cfg, data );

    /* Initialize sqlite functions */
    initialize_db( bfrs->sql_buffer, data );

    /*
     *
     *
     *
     *  TO BE REMOVED!!!!!!!!!!!!!!!!!!!!!!
     *
     *
     */
    int db_len =  get_db_len();
    printf( "number of entries: %d\n", db_len );

    int db_ret = dump_db_entries();

    if ( db_ret < 0 )
    {
        log_error( "Error opening sqlite3 file" );
    }
    else if ( db_ret )
    {
        log_error( "Some sort of SQL error occurred" );
    }
    else
    {
        for ( int i = 0; i < db_len; i++ )
        {
            fprintf( stdout,
            "output %d of %d (device name, mqtt topic," \
            " device type): %s %s %d\n",
            i, db_len,
            data[i].dev_name, data[i].mqtt_topic, data[i].dev_type );
        }
    }

    update_db_entry( "newg", "simbulz", NULL, NULL, -1 );

    /*
     *
     * END OF TO BE REMOVED!!!!!!!!!!!!!!!!!
     */

    /* Initialize mqtt listener */
    char port_str[10];
    sprintf( port_str, "%d", cfg->mqtt_port );
    int sockfd_mqtt = open_nb_socket( cfg->mqtt_server, port_str );

    if ( sockfd_mqtt < 0 )
    {
        perror("Failed to open socket: ");
        log_error( "Failed to open socket" );

        /* Cleanup and exit, this is a big deal. */
        //exit_example(EXIT_FAILURE, sockfd, NULL);
        cleanup( lg, bfrs, cfg, data, NULL );
        return 1;

    }
    log_trace( "mqtt socket established" );

    log_debug( "Initializing mqtt server itself" );

    struct mqtt_client *client = (struct mqtt_client*)malloc(
        sizeof(struct mqtt_client)
    );

    int mqtt_stat = initialize_mqtt( client, sockfd_mqtt, bfrs->send_buffer,
                                     bfrs->receive_buffer, cfg );


    /* Finally, initialize the kisslight server itself */
    int sockfd = create_server_socket( cfg->port );

    /*
     * If sockfd initialization is successful,
     * run the server.
     */
    if ( ! (sockfd < 0) )
    {
        log_trace( "server socket established" );

        /*
         * Time to Poll and run the server's network loop.
         */

        log_trace( "server exiting" );
    }
    else
    {
        perror("Error creating sockfd");
        log_error( "Error creating sockfd" );

        /* Cleanup and exit, this is a big deal. */
        cleanup( lg, bfrs, cfg, data, client );
        return 1;
    }

    /*
     * From this point, it can be safely assumed that
     * if this point is reached, it is time to wrap up.
     */
    cleanup( lg, bfrs, cfg, data, client);

    return 0;
}
