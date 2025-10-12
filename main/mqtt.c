/* MQTT (over TCP) Example
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "mqtt.h"
#include "local_time.h"
#include "weather.h"

// PRIVATE VARIABLE
static char Previous_message[32];
static uint8_t Flag_Active_Server;

// PRIVATE FUNCTION
static void log_error_if_nonzero(const char*, int);
static void mqtt_app_start(void);
void incoming_mqtt_handler(esp_mqtt_event_handle_t);
static void update_weather_module(char *);
static uint8_t dbl_dig_str_to_int(char, char);
void add_null_to_str(char *, char *, uint8_t);

static const char *TAG = "WEATHER_STATION: MQTT";

static esp_mqtt_client_handle_t client;


/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        //msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC_SUB_NOTIFY, "Reboot", 0, 1, 0);
        //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_subscribe(client, MQTT_TOPIC_DATA_UPDATE, 0);
        ESP_LOGI(TAG, "Subscribed successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        //msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        //msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC_DATA_BOOTUP, 0, 0 ,0);
        //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        incoming_mqtt_handler(event);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}


// PUBLIC Functions

void Mqtt__Start(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    strcpy(Previous_message, "0");
    Flag_Active_Server = 0;
    mqtt_app_start();
}

void Mqtt__Bootup_msgs(char *device_number) {
    // Request server to publish weather data and publish current version number
    Mqtt__Publish("dev_bootup", device_number);
}

void Mqtt__Subscribe(char *topic) {
    int msg_id = esp_mqtt_client_subscribe(client, topic, 0);
    ESP_LOGI(TAG, "Subscribed successful, msg_id=%d", msg_id);

}

void Mqtt__Publish(char *topic, char *msg) {
    esp_mqtt_client_publish(client, topic, msg, 0, 1, 0);
}

uint8_t Mqtt__Get_server_status(void) {
    return Flag_Active_Server;
}

void Mqtt__Reset_server_status(void) {
    Flag_Active_Server = 0;
}

// Insert timestamp and publish debug message
void Mqtt__Send_debug_msg(char *msg) {
    char time_str[8];
    Local_Time__Get_current_time_str(time_str);
    // combine time and message with a space between
    // combine the first 8 chars of time_str with msg
    char ret_msg[32];
    for(uint8_t i_char=0; i_char<8; i_char++) {
        if(time_str[i_char] == '\0') {
            time_str[i_char] = ' ';
        }
        ret_msg[i_char] = time_str[i_char];
    }
    ret_msg[8] = ' ';  // add space between time and message
    uint8_t i_msg;
    for(uint8_t i_char=9; i_char<32; i_char++) {
        i_msg = i_char - 9;
        ret_msg[i_char] = msg[i_msg];
        if(msg[i_msg] == '\0') {
            break;
        }
    }

    // char ret_msg[] = {time_str, ': ', msg};
    // ESP_LOGI(TAG, ret_msg);
    esp_mqtt_client_publish(client, "debug", ret_msg, 0, 1, 0);
}

// PRIVATE Functions

void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

// Handle incoming MQTT string message
void incoming_mqtt_handler(esp_mqtt_event_handle_t event) {
    char topic_str[event->topic_len + 1];
    char data_str[event->data_len + 1];
    add_null_to_str(topic_str, event->topic, event->topic_len);
    add_null_to_str(data_str, event->data, event->data_len);
    
    // Topic string applies to weather view of this device
    if(strcmp(topic_str, MQTT_TOPIC_DATA_UPDATE)==0) {
        // Set flag indicates receiving communicatings from server
        Flag_Active_Server = 1;
        // Data is different from previous message, update stored weather data
        if(strcmp(data_str, Previous_message)!= 0) {
            update_weather_module(data_str);
            strcpy(Previous_message, data_str);
        }
    }
    else if(strcmp(topic_str, MQTT_TOPIC_FW_VERSION)==0) {
        // compare payload to this version. if different, get OTA
    }
}


// Compare FW version number from server with this FW version
void compare_version(char * data_str) {
    // Version number is 1st char of str
    uint8_t version = dbl_dig_str_to_int('0', data_str[0]);

    if(version != FW_VERSION_NUM) {
        // Run OTA get updated FW
    }
}

// Incoming MQTT update view message. 1st char=0: current weather
//                                    1st char=1: forecast, includes max temp,precip,moon
void update_weather_module(char * data_str) {
    // API is 1st char of str
    uint8_t api = dbl_dig_str_to_int('0', data_str[0]);

    // Will send the following to view module to update values
    uint8_t payload[MAX_NUM_PAYLOAD_BYTES];
    uint8_t payload_len = 0;
    // Current temp (periodic through day)
    if (api == 0) {
        uint8_t temp_current = dbl_dig_str_to_int(data_str[1], data_str[2]);
        payload[0] = temp_current;
        payload_len = 1;
    }

    // Forecast today
    else if (api == 1) {
        // First byte=how many days forecasting
        uint8_t num_days = (data_str[1] - '0');
        payload[0] = num_days;
        payload_len = 1;

        uint8_t idx_data = 2;
        uint8_t idx_payload = 1;
        for(uint8_t day = 0; day<num_days; day++) {
            // idx_data += (day * 5);
            uint8_t temp_max = dbl_dig_str_to_int(data_str[idx_data], data_str[idx_data+1]);
            uint8_t precip = dbl_dig_str_to_int(data_str[idx_data+2], data_str[idx_data+3]);
            uint8_t moon = (data_str[idx_data+4] - '0');
            idx_data = idx_data + 5;

            // uint8_t idx_payload = ((day * 3) + 1);
            payload[idx_payload] = temp_max;
            payload[idx_payload+1] = precip;
            payload[idx_payload+2] = moon;
            idx_payload += 3;
        }
        payload_len = idx_payload + 1;
    }

    // Update display with newly receieved weather data
    Weather__Update_values(api, payload, payload_len);
}

// Convert char(s) to int
uint8_t dbl_dig_str_to_int(char dig1, char dig2) {
    char temp_digit[2];
    temp_digit[0] = dig1;
    temp_digit[1] = dig2;
    uint8_t dbl_dig_int = (uint8_t) strtol(temp_digit, NULL, 0);
    return(dbl_dig_int);
}

void add_null_to_str(char * ret_str, char * input_str, uint8_t str_len) {
    strncpy(ret_str, input_str, str_len);
    ret_str[str_len] = '\0';
}