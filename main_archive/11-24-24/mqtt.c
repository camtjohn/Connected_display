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
#include "view.h"

// PRIVATE VARIABLE

// PRIVATE FUNCTION
static void mqtt_app_start(void);
static void update_data_view(char *);
static uint8_t dbl_dig_str_to_int(char, char);


static const char *TAG = "MQTT_WEATHER";

static esp_mqtt_client_handle_t client;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

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
        update_data_view(event->data);
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

    mqtt_app_start();
}

void Mqtt__Publish(char *topic, char *msg) {
    esp_mqtt_client_publish(client, topic, msg, 0, 1, 0);
}

void Mqtt__Subscribe(char *topic) {
    int msg_id = esp_mqtt_client_subscribe(client, topic, 0);
    ESP_LOGI(TAG, "Subscribed successful, msg_id=%d", msg_id);

}

// PRIVATE Functions
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

// Handle incoming MQTT string message. 1st char=0: current weather
//                                      1st char=1: forecast, includes max temp,precip,moon
void update_data_view(char * data_str) {
    // API is 1st char of str
    uint8_t api = dbl_dig_str_to_int('0', data_str[0]);

    // Will send the following to view module to update values
    uint8_t short_arr[MAX_NUM_PAYLOAD_BYTES];
    uint8_t arr_len = 0;

    // Current temp (periodic through day)
    if (api == 0) {
        uint8_t temp_current = dbl_dig_str_to_int(data_str[1], data_str[2]);
        short_arr[0] = temp_current;
        arr_len = 1;
    }

    // Forecast today
    else if (api == 1) {
        // Translate string to int     
        uint8_t temp_max = dbl_dig_str_to_int(data_str[1], data_str[2]);
        uint8_t precip_today = dbl_dig_str_to_int(data_str[3], data_str[4]);
        uint8_t moon_today = dbl_dig_str_to_int(data_str[5], data_str[6]);
        
        short_arr[0] = temp_max;
        short_arr[1] = precip_today;
        short_arr[2] = moon_today;
        arr_len = 3;
    }

    // Update display with newly receieved weather data
    View__Update_values_8bit(api, short_arr, arr_len);
}

// Convert char(s) to int
uint8_t dbl_dig_str_to_int(char dig1, char dig2) {
    char temp_digit[2];
    temp_digit[0] = dig1;
    temp_digit[1] = dig2;
    uint8_t dbl_dig_int = (uint8_t) strtol(temp_digit, NULL, 0);
    return(dbl_dig_int);
}