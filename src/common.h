#ifndef COMMON_H_
#define COMMON_H_

#define CONFLOCATION ((const char *)"/etc/kisslight.ini")
#define MAX_DEVICES 50
#define KL_VERSION 0.3

/* Includes in case the compiler complains */
#include <pthread.h>
#include <semaphore.h>

/* Useful constants regarding database reside here */
enum {
    DB_DATA_LEN = 512
};

/* Process args */
void process_args( int argc, char **argv );

/* configuration parser related */
int initialize_conf_parser();

/* configuration parser struct */
typedef struct
{
    int port;
    int buffer_size;
    const char *mqtt_server;
    int mqtt_port;
    int recv_buff;
    int snd_buff;
    int topic_buff;
    int app_msg_buff;
    const char *db_loc;
    int db_buff;
    int max_dev_count;
} config;

/* database related */

/* database output struct */
typedef struct
{
    char dev_name[DB_DATA_LEN];
    char mqtt_topic[DB_DATA_LEN];
    int dev_type;
    int dev_state;

    /*
     * Independent of the database,
     * it just keeps tracks of various topics
     */
    char cmnd_topic[DB_DATA_LEN];
    char stat_topic[DB_DATA_LEN];

    /*
     * for database usage,
     * for old entries.
     */
    char odev_name[DB_DATA_LEN];
    char omqtt_topic[DB_DATA_LEN];

} db_data;

int initialize_db( char *sql_buffer, db_data *dat, int *to_chng,
                   pthread_mutex_t *lck, sem_t *mtx );

int get_db_len();

int insert_db_entry( char *dev_name, char *mqtt_topic, const int type );

int dump_db_entries();

int update_db_entry( char *odev_name, char *ndev_name,
                     char *omqtt_topic, char *nmqtt_topic,
                     const int ndev_type );

int update_db_dev_state( char *dev_name, char *mqtt_topic,
                         const int new_state );

int delete_db_entry( char *dev_name, char *mqtt_topic );

void *db_updater( void* args );

#endif
