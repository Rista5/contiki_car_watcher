
#include "contiki.h"
#include "net/routing/routing.h"
#include "mqtt.h"
#include "mqtt-prop.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/sicslowpan.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "lib/sensors.h"
#include "dev/button-hal.h"
#include "dev/leds.h"
#include "os/sys/log.h"
#include "car-watcher.h"

// #include "clock.h"
#include "sensor.h"
#include "job.h"

#include "sensors/temperature_sensor.h"
#include "sensors/speed_sensor.h"
#include "sensors/rpm_sensor.h"
#include "sensors/gas_sensor.h"
#include "sensors/battery_sensor.h"

#include <string.h>
#include <strings.h>
#include <stdarg.h>
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "mqtt-client"
#ifdef MQTT_CLIENT_CONF_LOG_LEVEL
#define LOG_LEVEL MQTT_CLIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_NONE
#endif
/*---------------------------------------------------------------------------*/
/* MQTT broker address */
#ifdef MQTT_CLIENT_CONF_BROKER_IP_ADDR
#define MQTT_CLIENT_BROKER_IP_ADDR MQTT_CLIENT_CONF_BROKER_IP_ADDR
#else
#define MQTT_CLIENT_BROKER_IP_ADDR "fd00::1"
#endif
/*---------------------------------------------------------------------------*/
#ifdef MQTT_CLIENT_CONF_ORG_ID
#define MQTT_CLIENT_ORG_ID MQTT_CLIENT_CONF_ORG_ID
#else
#define MQTT_CLIENT_ORG_ID "asd"
#endif
/*---------------------------------------------------------------------------*/
/* MQTT token */
#ifdef MQTT_CLIENT_CONF_AUTH_TOKEN
#define MQTT_CLIENT_AUTH_TOKEN MQTT_CLIENT_CONF_AUTH_TOKEN
#else
#define MQTT_CLIENT_AUTH_TOKEN "asd"
#endif
/*---------------------------------------------------------------------------*/
static const char *broker_ip = MQTT_CLIENT_BROKER_IP_ADDR;

#ifdef MQTT_CLIENT_CONF_USERNAME
#define MQTT_CLIENT_USERNAME MQTT_CLIENT_CONF_USERNAME
#else
#define MQTT_CLIENT_USERNAME "use-token-auth"
#endif
/*---------------------------------------------------------------------------*/
/*
 * A timeout used when waiting for something to happen (e.g. to connect or to
 * disconnect)
 */
#define STATE_MACHINE_PERIODIC (CLOCK_SECOND >> 1)
/*---------------------------------------------------------------------------*/
/* Connections and reconnections */
#define RETRY_FOREVER 0xFF
#define RECONNECT_INTERVAL (CLOCK_SECOND * 2)

#define RECONNECT_ATTEMPTS RETRY_FOREVER
#define CONNECTION_STABLE_TIME (CLOCK_SECOND * 5)
static struct timer connection_life;
static uint8_t connect_attempt;
/*---------------------------------------------------------------------------*/
/* Various states */
static uint8_t state;
#define STATE_INIT 0
#define STATE_REGISTERED 1
#define STATE_CONNECTING 2
#define STATE_CONNECTED 3
#define STATE_PUBLISHING 4
#define STATE_DISCONNECTED 5
#define STATE_NEWCONFIG 6
#define STATE_CONFIG_ERROR 0xFE
#define STATE_ERROR 0xFF
/*---------------------------------------------------------------------------*/
#define CONFIG_ORG_ID_LEN 32
#define CONFIG_TYPE_ID_LEN 32
#define CONFIG_AUTH_TOKEN_LEN 32
#define CONFIG_EVENT_TYPE_ID_LEN 32
#define CONFIG_CMD_TYPE_LEN 8
#define CONFIG_IP_ADDR_STR_LEN 64
/*---------------------------------------------------------------------------*/
/* A timeout used when waiting to connect to a network */
#define NET_CONNECT_PERIODIC (CLOCK_SECOND >> 2)
#define NO_NET_LED_DURATION (NET_CONNECT_PERIODIC >> 1)
/*---------------------------------------------------------------------------*/
/* Default configuration values */
#define DEFAULT_TYPE_ID "client1"
#define DEFAULT_EVENT_TYPE_ID "status"
#define DEFAULT_SUBSCRIBE_CMD_TYPE "+"
#define DEFAULT_BROKER_PORT 1883
#define DEFAULT_PUBLISH_INTERVAL (5 * CLOCK_SECOND)
#define DEFAULT_KEEP_ALIVE_TIMER 60
#define DEFAULT_RSSI_MEAS_INTERVAL (CLOCK_SECOND * 30)
/*---------------------------------------------------------------------------*/
PROCESS_NAME(mqtt_client_process);
AUTOSTART_PROCESSES(&mqtt_client_process);
/*---------------------------------------------------------------------------*/
/**
 * \brief Data structure declaration for the MQTT client configuration
 */
typedef struct mqtt_client_config
{
    char org_id[CONFIG_ORG_ID_LEN];
    char type_id[CONFIG_TYPE_ID_LEN];
    char auth_token[CONFIG_AUTH_TOKEN_LEN];
    char event_type_id[CONFIG_EVENT_TYPE_ID_LEN];
    char broker_ip[CONFIG_IP_ADDR_STR_LEN];
    char cmd_type[CONFIG_CMD_TYPE_LEN];
    clock_time_t pub_interval;
    int def_rt_ping_interval;
    uint16_t broker_port;
} mqtt_client_config_t;
/*---------------------------------------------------------------------------*/
/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE 32
/*---------------------------------------------------------------------------*/
#define BUFFER_SIZE 64
static char client_id[BUFFER_SIZE];
static char pub_topic[BUFFER_SIZE];
static char sub_topic[BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
/*The main MQTT buffers*/
#define APP_BUFFER_SIZE 512
static struct mqtt_connection conn;
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
static struct mqtt_message *msg_ptr = 0;
static struct etimer publish_periodic_timer;
static char *buf_ptr;
static uint16_t seq_nr_value = 0;
/*---------------------------------------------------------------------------*/
static mqtt_client_config_t conf;
/*---------------------------------------------------------------------------*/
PROCESS(mqtt_client_process, "MQTT Client");
/*---------------------------------------------------------------------------*/
#define MAX_EVENT_NUM_LEN 10
#define CAR_IS_ON 1
#define CAR_IS_OFF 0

#define WARNING_LED LEDS_RED
#define MISUSE_LED LEDS_YELLOW
#define LED_OFF_TIMER (CLOCK_SECOND * 10)
#define LED_ON_TIMER (CLOCK_SECOND * 1)

#define WARNING 1
#define MISUSE (1 << 1)
#define WARNING_ON (1 << 2)
#define MISUSE_ON (1 << 3)

static unsigned int led_status = 0;
static struct etimer led_timer;

static unsigned char car_status = CAR_IS_ON;

static sensor_array sensor_arr = 
{
    .count = 0
};

#define TMP_BUFFER_SIZE 100

#define UNDEFINED_ROUTING_KEY           "undefined"
#define SENSOR_READING_STRING           "sensor_reading"
#define STARTED_SENDING_STRING          "started_sending"
#define STOPPED_SENDING_STRING          "stopped_sending"
#define WARNING_STARTED_STRING          "warn_started"
#define WARNING_STOPPED_STIRNG          "warn_stopped"
#define CAR_OFF_STRING                  "car_disabled"
#define CAR_ENABLED_STRING              "car_enabled"
#define GET_PUBLISH_INTERVAL_STRING     "publish_interval"
#define INTERVAL_CHANGED_STRING         "publish_interval_changed"

/*---------------------------------------------------------------------------*/
void publish_sensor_reading(void);
void init_sensors();
void process_server_message(const unsigned char * msg);

int  send_publish_interval();
void stop_warning();
void turn_off_warning_led();
void show_warning();
void display_warning();
void check_for_misuse(sensor_array* sensor_arr);

static int construct_pub_topic(int msg_event);
/*---------------------------------------------------------------------------*/
char* get_message_string(int message_num)
{
    switch (message_num)
    {
    case SENSOR_READING:
        return SENSOR_READING_STRING;
        break;
    case START_SENDING:
        return STARTED_SENDING_STRING;
        break;
    case STOP_SENDING:
        return STOPPED_SENDING_STRING;
        break;
    case SEND_WARNING:
        return WARNING_STARTED_STRING;
        break;
    case STOP_WARNING:
        return WARNING_STOPPED_STIRNG;
        break;
    case TURN_OFF_CAR:
        return CAR_OFF_STRING;
        break;
    case ENABLE_CAR_ENGINE:
        return CAR_ENABLED_STRING;
        break;
    case GET_SENSOR_PUBLISH_INTERVAL:
        return GET_PUBLISH_INTERVAL_STRING;
        break;
    case SET_READING_INTERVAL:
        return INTERVAL_CHANGED_STRING;
        break;
    default:
        return UNDEFINED_ROUTING_KEY;
        break;
    }
}

void extract_string_number(char * dst, const char * src, int max_len)
{
    char *ptr;
    int counter = 0;
    ptr = strstr(src, ":");
    ptr++;

    while(*ptr < '0' || *ptr > '9')
    {
        ptr++;
    }
    
    while(*ptr >= '0' && *ptr <= '9' && counter < max_len)
    {
        dst[counter++] = *ptr;
        ptr++;
    }

    if (counter < max_len)
    {
        dst[counter] = '\0';
    }
    else
    {
       dst[0] = '0';
       dst[1] = '\0';
    }
}

int extract_message_number(const char* msg)
{
    char * event_string_ptr;
    char event_num[MAX_EVENT_NUM_LEN];
    unsigned int event;

    event_string_ptr = strstr(msg, EVENT_KEY);
    
    if(event_string_ptr == NULL)
    {
        printf("Wrong message format, could not find event string.\n");
        return UNDEFINED_MESSAGE;
    }
    
    event_string_ptr += strlen(EVENT_KEY);
    extract_string_number(event_num, event_string_ptr, MAX_EVENT_NUM_LEN);

    event = atoi(event_num);
    return event;
}

int publish_message(int msg_event, char* data)
{
    mqtt_status_t status;
    
    if(construct_pub_topic(msg_event))
    {
        status = mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)data,
                     strlen(data), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);
        return status == MQTT_STATUS_OK;
    }
    return 0;
}

int write_basic_response(int msg_event, char* buff, int max_len)
{
    char* event;
    int len;

    event = get_message_string(msg_event);

    len = snprintf(buff, max_len, 
                "{"
                EVENT_KEY":%d,"
                "\"message\":\"%s\""
                "}", msg_event, event);

    printf("WRITTER: %s\n", buff);
    if (len < 0 || len >= max_len)
    {
        LOG_ERR("Buffer too short. Have %d, need %d + \\0\n", max_len,
                len);
        return 0;
    }
    return 1;
}

int send_basic_response(int msg_event)
{
    char buff[TMP_BUFFER_SIZE];

    if(write_basic_response(msg_event, buff, TMP_BUFFER_SIZE))
    {
        return publish_message(msg_event, buff);
    }
    printf("MESSAGE_SENT: %s\n", buff);
    return 0;
}

void turn_off_car()
{
    car_status = CAR_IS_OFF;
    printf("OWNER HAS DISABLED YOUR CAR\n");
    send_basic_response(TURN_OFF_CAR);
}

void turn_on_car()
{
    car_status = CAR_IS_ON;
    printf("OWNER HAS ENABLED YOU TO USE CAR\n");
    send_basic_response(ENABLE_CAR_ENGINE);
}

void stop_sending_data()
{
    etimer_stop(&publish_periodic_timer);
    send_basic_response(STOP_SENDING);
}

void start_sending_data()
{
    if(etimer_expired(&publish_periodic_timer))
    {
        printf("started sending data\n");
        etimer_set(&publish_periodic_timer, conf.pub_interval);
    }
}

void queue_start_sending_data()
{
    job_request job;

    if(etimer_expired(&publish_periodic_timer))
    {
        job.func = start_sending_data;
        job.argv = NULL;
        add_job(&job);
        send_basic_response(START_SENDING); //response is sent immediately because if message is sent from another process, message gets corrupted
    }
}

void display_warning()
{
    if(led_status & (unsigned int) (WARNING_ON))
        return;
    printf("\nRED LED turns ON!\n");
    leds_on(WARNING_LED);
    led_status = led_status | (unsigned int) (WARNING_ON);
    etimer_set(&led_timer, LED_ON_TIMER);
}

void show_warning()
{
    printf("Owner has sent a warning!\n");
    led_status = led_status | (unsigned int) (WARNING);
    display_warning();
    send_basic_response(SEND_WARNING);
}

void turn_off_warning_led()
{
    if (!(led_status & (unsigned int)WARNING_ON))
        return;
    printf("\nRED LED turns OFF!\n");
    leds_off(WARNING_LED);
    led_status = led_status & (~(unsigned int)WARNING_ON);
}

void stop_warning()
{
    printf("Owner has removed a warning!\n");
    turn_off_warning_led();
    led_status = led_status & (~(unsigned int)(WARNING | WARNING_ON));
    etimer_stop(&led_timer);
    send_basic_response(STOP_WARNING);
}

void start_misuse_warning()
{
    if(!(led_status & (unsigned int) (MISUSE)))
    {
        printf("\nYELLOW LED turns ON!\n");
    }
    led_status = led_status | (unsigned int) (MISUSE);
    leds_on(MISUSE_LED);
}

void stop_misuse_warning()
{
    if(led_status & (unsigned int) (MISUSE))
    {
        printf("\nYELLOW LED turns OFF!\n");
    }
    led_status = led_status & (~(unsigned int)MISUSE);
    leds_off(MISUSE_LED);
}

void check_for_misuse(sensor_array* sens_arr)
{
    char status = false;
    int i;

    for(i = 0; i < sens_arr->count; i++)
    {
        if (sens_arr->arr[i].constraint != NULL 
            && sens_arr->arr[i].constraint->status != CONSTRAINT_MET)
        {
            status = true;
            break;
        }
    }
    
    if(status)
    {
        start_misuse_warning();
    }
    else if (led_status & (unsigned int) (MISUSE))
    {
        stop_misuse_warning();
    }
}

int send_publish_interval()
{
    int remaining = TMP_BUFFER_SIZE;
    int len;
    char buff[TMP_BUFFER_SIZE];

    len = snprintf(buff, remaining,
                   "{"
                   EVENT_KEY":%d,"
                   "\"message\":\"%s\","
                   "\"publish_interval\": %d"
                   "}", GET_SENSOR_PUBLISH_INTERVAL,
                   get_message_string(GET_SENSOR_PUBLISH_INTERVAL),
                   (int)conf.pub_interval);
    if (len < 0 || len >= remaining)
    {
        LOG_ERR("Buffer too short. Have %d, need %d + \\0\n", remaining,
                len);
        return 0;
    }

    return publish_message(GET_SENSOR_PUBLISH_INTERVAL, buff);
}

void change_interval(void* change_args)
{
    printf("changing publish interval to %d\n", (int)conf.pub_interval);
    etimer_reset_with_new_interval(&publish_periodic_timer, conf.pub_interval);
}

#define BUFFER_NUM_LEN 10
void change_reading_interval(const unsigned char* msg)
{
    int num_of_sec;
    char* ptr;
    char num_buff[BUFFER_NUM_LEN];
    struct job_request job;
    ptr = strstr((char *)msg, CHANGE_INTERVAL_KEY);

    if (ptr == NULL)
        return;
    
    ptr += strlen(CHANGE_INTERVAL_KEY);

    extract_string_number(num_buff, ptr, BUFFER_NUM_LEN);
    num_of_sec = atoi(num_buff);
    if(num_of_sec == 0)
        return;

    conf.pub_interval = CLOCK_SECOND * num_of_sec;
    job.func = change_interval;
    job.argv = NULL;
    add_job(&job);
    send_basic_response(SET_READING_INTERVAL); //response is sent immediately because if message is sent from another process, message gets corrupted
                                               // maybe should also send value that was set
}

void process_server_message(const unsigned char * msg)
{
    int msg_num = UNDEFINED_MESSAGE;

    msg_num = extract_message_number((char*) msg);

    printf("MESSAGE NUMBER %d\n", msg_num);

    switch (msg_num)
    {
    case START_SENDING:
        queue_start_sending_data();
        break;
    case STOP_SENDING:
        stop_sending_data();
        break;
    case SEND_WARNING:
        show_warning();
        break;
    case STOP_WARNING:
        stop_warning();
        break;
    case TURN_OFF_CAR:
        turn_off_car();
        break;
    case ENABLE_CAR_ENGINE:
        turn_on_car();
        break;
    case GET_SENSOR_PUBLISH_INTERVAL:
        send_publish_interval();
        break;
    case SET_READING_INTERVAL:
        change_reading_interval(msg);
        break;
    default:
        printf("Undefined message number: %d\n", msg_num);
        break;
    }
}

int print_sensor_value_to_buff(char *buff, int remaining, char *key, float value)
{
    int len;
    
    len = snprintf(buff, remaining, "\"%s\":%8.3f,", key, value);

    return len;
}

void publish_sensor_reading(void)
{
    int len;
    int remaining = APP_BUFFER_SIZE;
    int i;
    char* sensor_name;
    float sensor_value;

    seq_nr_value++;
    buf_ptr = app_buffer;

    len = snprintf(buf_ptr, remaining,
                   "{"
                   EVENT_KEY":%d,"
                   "\"data\":{"
                   "\"Platform\":\"" CONTIKI_TARGET_STRING "\","
                   "\"Seq\":%d,",
                   SENSOR_READING, seq_nr_value);
    
    
    if (len < 0 || len >= remaining)
    {
        LOG_ERR("Buffer too short. Have %d, need %d + \\0\n", remaining,
                len);
        return;
    }

    remaining -= len;
    buf_ptr += len;

    len = snprintf(buf_ptr, remaining, "\"sens_data\":{");
    remaining -= len;
    buf_ptr += len;

    for(i = 0; i < sensor_arr.count; i++)
    {
        
        sensor_name = sensor_arr.arr[i].sensor_name();
        sensor_value = read_sensor_with_constraint_check(sensor_arr.arr[i]);

        len = print_sensor_value_to_buff(buf_ptr, remaining, sensor_name, sensor_value);

        if (len < 0 || len >= remaining)
        {
            LOG_ERR("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
            return;
        }

        remaining -= len;
        buf_ptr += len;
    }
    buf_ptr--; // remove last ","
    remaining++;

    snprintf(buf_ptr, remaining, "}}}");

    construct_pub_topic(SENSOR_READING);
    mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer,
                 strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);

    printf("Publish!\n");
}

void temperature_callback(char* name, float value, float min, float max)
{
    printf("Car temperature: %8.3f is exceeding constraints:\nmin:%8.3f \nmax:%8.3f\n", value, min, max);
}

void rpm_callback(char* name, float value, float min, float max)
{
    printf("Car rpm: %4.0f is exceeding constraints:\nmin:%4.0f \nmax:%4.0f\n", value, min, max);
}

void speed_callback(char* name, float value, float min, float max)
{
    printf("Car speed: %6.2f is exceeding constraints:\nmin:%6.2f \nmax:%6.2f\n", value, min, max);
}

void add_sensors_to_array()
{
    add_sensor(&sensor_arr, init_temp_sensor, read_temperature, temp_sensor_name);
    add_sensor(&sensor_arr, init_speed_sensor, read_speed, speed_sensor_name);
    add_sensor(&sensor_arr, init_rpm_sensor, read_rpm, rpm_sensor_name);
    add_sensor(&sensor_arr, init_gas_sensor, read_gas, gas_sensor_name);
    add_sensor(&sensor_arr, init_battery_sensor, read_battery_charge, battery_sensor_name);

    
    add_constraint(&sensor_arr, temp_sensor_name(), INVALID_TEMPERATURE, 100, 
        temperature_callback, AT_EVERY_READING);
    add_constraint(&sensor_arr, rpm_sensor_name(), INVALID_RPM, 3000.0, 
        rpm_callback, AT_EVERY_READING);
    add_constraint(&sensor_arr, speed_sensor_name(), INVALID_SPEED, 110, 
        speed_callback, AT_EVERY_READING);
}

void init_sensors()
{
    int i;
    add_sensors_to_array();

    for(i = 0; i < sensor_arr.count; i++)
    {
        sensor_arr.arr[i].init();
    }
}

static bool have_connectivity(void)
{
    if (uip_ds6_get_global(ADDR_PREFERRED) == NULL ||
        uip_ds6_defrt_choose() == NULL)
    {
        return false;
    }
    return true;
}

static void mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
    switch (event)
    {
    case MQTT_EVENT_CONNECTED:
    {
        LOG_DBG("Application has a MQTT connection\n");
        timer_set(&connection_life, CONNECTION_STABLE_TIME);
        state = STATE_CONNECTED;
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
    case MQTT_EVENT_CONNECTION_REFUSED_ERROR:
    {
        LOG_DBG("MQTT Disconnect. Reason %u\n", *((mqtt_event_t *)data));

        state = STATE_DISCONNECTED;
        process_poll(&mqtt_client_process);
        break;
    }
    case MQTT_EVENT_PUBLISH:
    {
        msg_ptr = data;

        /* Implement first_flag in publish message? */
        if (msg_ptr->first_chunk)
        {
            msg_ptr->first_chunk = 0;
            LOG_DBG("Application received publish for topic '%s'. Payload "
                    "size is %i bytes.\n",
                    msg_ptr->topic, msg_ptr->payload_chunk_length);
        }

        process_server_message(msg_ptr->payload_chunk);

        break;
    }
    case MQTT_EVENT_SUBACK:
    {
        LOG_DBG("Application is subscribed to topic successfully\n");
        break;
    }
    case MQTT_EVENT_UNSUBACK:
    {
        LOG_DBG("Application is unsubscribed to topic successfully\n");
        break;
    }
    case MQTT_EVENT_PUBACK:
    {
        LOG_DBG("Publishing complete.\n");
        break;
    }
    default:
        LOG_DBG("Application got a unhandled MQTT event: %i\n", event);
        break;
    }
}

static int construct_pub_topic(int msg_event)
{
    static int curr_routing_key = -1;

    if (curr_routing_key == msg_event)
        return 1;

    int len = snprintf(pub_topic, BUFFER_SIZE, "%s/server", client_id);
    
    if (len < 0 || len >= BUFFER_SIZE)
    {
        LOG_INFO("Pub Topic: %d, Buffer %d\n", len, BUFFER_SIZE);
        return 0;
    }

    curr_routing_key = msg_event;

    return 1;
}

static int construct_sub_topic(void)
{
    // subscribe to all messages forwared to this client
    int len = snprintf(sub_topic, BUFFER_SIZE, "server/%s", client_id);

    if (len < 0 || len >= BUFFER_SIZE)
    {
        LOG_INFO("Sub Topic: %d, Buffer %d\n", len, BUFFER_SIZE);
        return 0;
    }

    return 1;
}

static int construct_client_id(void)
{
    int len = snprintf(client_id, BUFFER_SIZE, CLIENT_ID);

    if (len < 0 || len >= BUFFER_SIZE)
    {
        LOG_ERR("Client ID: %d, Buffer %d\n", len, BUFFER_SIZE);
        return 0;
    }

    return 1;
}

static void init_mqtt_config(void)
{
    if (construct_client_id() == 0)
    {
        /* Fatal error. Client ID larger than the buffer */
        state = STATE_CONFIG_ERROR;
        return;
    }

    if (construct_sub_topic() == 0)
    {
        /* Fatal error. Topic larger than the buffer */
        state = STATE_CONFIG_ERROR;
        return;
    }

    if (construct_pub_topic(SENSOR_READING) == 0)
    {
        /* Fatal error. Topic larger than the buffer */
        state = STATE_CONFIG_ERROR;
        return;
    }

    seq_nr_value = 0;
    state = STATE_INIT;

    etimer_set(&publish_periodic_timer, 0);

    return;
}

static int init_config()
{
    /* Populate configuration with default values */
    memset(&conf, 0, sizeof(mqtt_client_config_t));

    memcpy(conf.org_id, MQTT_CLIENT_ORG_ID, strlen(MQTT_CLIENT_ORG_ID));
    memcpy(conf.type_id, DEFAULT_TYPE_ID, strlen(DEFAULT_TYPE_ID));
    memcpy(conf.auth_token, MQTT_CLIENT_AUTH_TOKEN, strlen(MQTT_CLIENT_AUTH_TOKEN));
    memcpy(conf.event_type_id, DEFAULT_EVENT_TYPE_ID, strlen(DEFAULT_EVENT_TYPE_ID));
    memcpy(conf.broker_ip, broker_ip, strlen(broker_ip));
    memcpy(conf.cmd_type, DEFAULT_SUBSCRIBE_CMD_TYPE, 1);

    conf.broker_port = DEFAULT_BROKER_PORT;
    conf.pub_interval = DEFAULT_PUBLISH_INTERVAL;
    conf.def_rt_ping_interval = DEFAULT_RSSI_MEAS_INTERVAL;

    return 1;
}

static void subscribe(void)
{
    mqtt_status_t status;
    printf("SUBSCRIBE called!\n");

    status = mqtt_subscribe(&conn, NULL, sub_topic, MQTT_QOS_LEVEL_0);

    LOG_DBG("Subscribing!\n");
    if (status == MQTT_STATUS_OUT_QUEUE_FULL)
    {
        LOG_ERR("Tried to subscribe but command queue was full!\n");
    }
}

static void connect_to_broker(void)
{
    /* Connect to MQTT server */
    mqtt_connect(&conn, conf.broker_ip, conf.broker_port,
                 (conf.pub_interval * 3) / CLOCK_SECOND,
                 MQTT_CLEAN_SESSION_ON);

    state = STATE_CONNECTING;
}

static void state_machine(void)
{
    switch (state)
    {
    case STATE_INIT:
        /* If we have just been configured register MQTT connection */
        mqtt_register(&conn, &mqtt_client_process, client_id, mqtt_event,
                      MAX_TCP_SEGMENT_SIZE);

        /* _register() will set auto_reconnect. We don't want that. */
        conn.auto_reconnect = 0;
        connect_attempt = 1;

        state = STATE_REGISTERED;
        LOG_DBG("Init MQTT version %d\n", MQTT_PROTOCOL_VERSION);
        /* Continue */
    case STATE_REGISTERED:
        if (have_connectivity())
        {
            /* Registered and with a public IP. Connect */
            LOG_DBG("Registered. Connect attempt %u\n", connect_attempt);
            connect_to_broker();
        }
        etimer_set(&publish_periodic_timer, NET_CONNECT_PERIODIC);
        return;
        break;
    case STATE_CONNECTING:
        /* Not connected yet. Wait */
        LOG_DBG("Connecting (%u)\n", connect_attempt);
        break;
    case STATE_CONNECTED:
    case STATE_PUBLISHING:
        /* If the timer expired, the connection is stable. */
        if (timer_expired(&connection_life))
        {
            /*
       * Intentionally using 0 here instead of 1: We want RECONNECT_ATTEMPTS
       * attempts if we disconnect after a successful connect
       */
            connect_attempt = 0;
        }

        if (mqtt_ready(&conn) && conn.out_buffer_sent)
        {
            /* Connected. Publish */
            if (state == STATE_CONNECTED)
            {
                subscribe();
                state = STATE_PUBLISHING;
            }
            else
            {
                publish_sensor_reading();
                check_for_misuse(&sensor_arr);
            }
            etimer_set(&publish_periodic_timer, conf.pub_interval);
            /* Return here so we don't end up rescheduling the timer */
            return;
        }
        else
        {
            /*
       * Our publish timer fired, but some MQTT packet is already in flight
       * (either not sent at all, or sent but not fully ACKd).
       *
       * This can mean that we have lost connectivity to our broker or that
       * simply there is some network delay. In both cases, we refuse to
       * trigger a new message and we wait for TCP to either ACK the entire
       * packet after retries, or to timeout and notify us.
       */
            LOG_DBG("Publishing... (MQTT state=%d, q=%u)\n", conn.state,
                    conn.out_queue_full);
        }
        break;
    case STATE_DISCONNECTED:
        LOG_DBG("Disconnected\n");
        if (connect_attempt < RECONNECT_ATTEMPTS ||
            RECONNECT_ATTEMPTS == RETRY_FOREVER)
        {
            /* Disconnect and backoff */
            clock_time_t interval;
            mqtt_disconnect(&conn);

            connect_attempt++;

            interval = connect_attempt < 3 ? RECONNECT_INTERVAL << connect_attempt : RECONNECT_INTERVAL << 3;

            LOG_DBG("Disconnected. Attempt %u in %lu ticks\n", connect_attempt, interval);

            etimer_set(&publish_periodic_timer, interval);

            state = STATE_REGISTERED;
            return;
        }
        else
        {
            /* Max reconnect attempts reached. Enter error state */
            state = STATE_ERROR;
            LOG_DBG("Aborting connection after %u attempts\n", connect_attempt - 1);
        }
        break;
    case STATE_CONFIG_ERROR:
        /* Idle away. The only way out is a new config */
        LOG_ERR("Bad configuration.\n");
        return;
    case STATE_ERROR:
    default:
        /*
        * 'default' should never happen.
        *
        * If we enter here it's because of some error. Stop timers. The only thing
        * that can bring us out is a new config event
        */
        LOG_ERR("Default case: State=0x%02x\n", state);
        return;
    }

    /* If we didn't return so far, reschedule ourselves */
    etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
}


PROCESS_THREAD(mqtt_client_process, ev, data)
{

    PROCESS_BEGIN();

    printf("Car watcher started\n");

    if (init_config() != 1)
    {
        PROCESS_EXIT();
    }

    init_sensors();

    init_mqtt_config();

    init_job_list();

    while (1)
    {

        PROCESS_YIELD();

        if (ev == PROCESS_EVENT_TIMER && data == &publish_periodic_timer)
        {
            state_machine();
        }

        process_jobs();
    }

    PROCESS_END();
}
