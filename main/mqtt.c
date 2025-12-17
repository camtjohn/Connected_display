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
#include "mqtt_protocol.h"
#include "device_config.h"
#include "local_time.h"
#include "weather.h"
#include "ota.h"
#include "main.h"

// PRIVATE VARIABLE
static uint8_t Previous_weather_message[(3*3) + MQTT_PROTOCOL_HEADER_SIZE]; // Max size for weather 3day forecast + header
static uint8_t Flag_Active_Server;
static char *client_cert_buffer = NULL;  // Dynamically allocated from NVS
static char *client_key_buffer = NULL;   // Dynamically allocated from NVS
static char weather_topic_with_zip[24] = {0};  // Static buffer for weather topic with zipcode

// PRIVATE FUNCTION
static void log_error_if_nonzero(const char*, int);
static void mqtt_app_start(void);
static void handle_mqtt_connected(esp_mqtt_client_handle_t client);
static void incoming_mqtt_handler(esp_mqtt_event_handle_t);
static void process_current_weather(const uint8_t *payload, uint8_t payload_len);
static void process_forecast_weather(const uint8_t *payload, uint8_t payload_len);
static void check_and_trigger_ota_update(uint8_t server_version);

static const char *TAG = "WEATHER_STATION: MQTT";

static esp_mqtt_client_handle_t client;

// Only CA certificate is embedded in binary
extern const uint8_t ca_crt_start[] asm("_binary_ca_crt_start");
extern const uint8_t ca_crt_end[]   asm("_binary_ca_crt_end");
// Client cert and key are loaded from NVS

/*
 * @brief Handle MQTT connected event
 * 
 * Subscribes to weather and device-specific topics, and sends bootup device config
 * 
 * @param client MQTT client handle
 */
static void handle_mqtt_connected(esp_mqtt_client_handle_t client) {
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    
    int msg_id;
    // Build weather topic with zipcode from NVS
    char zipcode[8] = {0};
    if (Device_Config__Get_Zipcode(zipcode, sizeof(zipcode)) == 0) {
        // Append zipcode to base topic (no separators) and store in static variable
        snprintf(weather_topic_with_zip, sizeof(weather_topic_with_zip), "%s%s", MQTT_TOPIC_WEATHER_BASE, zipcode);
        ESP_LOGI(TAG, "Subscribing to weather topic: %s", weather_topic_with_zip);
        msg_id = esp_mqtt_client_subscribe(client, weather_topic_with_zip, 0);
    } else {
        ESP_LOGE(TAG, "Failed to read zipcode from NVS, subscribing to base topic only");
        snprintf(weather_topic_with_zip, sizeof(weather_topic_with_zip), "%s", MQTT_TOPIC_WEATHER_BASE);
        msg_id = esp_mqtt_client_subscribe(client, MQTT_TOPIC_WEATHER_BASE, 0);
    }

    // Subscribe to device-specific topic
    esp_mqtt_client_subscribe(client, MQTT_TOPIC_DEV_SPECIFIC, 0);

    // Send device config message (binary) from NVS
    uint8_t config_msg[64];
    char device_name[16];
    if (Device_Config__Get_Name(device_name, sizeof(device_name)) == 0 &&
        Device_Config__Get_Zipcode(zipcode, sizeof(zipcode)) == 0) {
        const char *config_strings[] = {device_name, zipcode};
        ESP_LOGI(TAG, "Building device config with: '%s' and '%s'", device_name, zipcode);
        int config_len = mqtt_protocol_build_device_config(config_strings, 2, config_msg, sizeof(config_msg));
        if (config_len > 0) {
            ESP_LOGI(TAG, "Successfully built device config message: %d bytes", config_len);
            Mqtt__Publish(MQTT_TOPIC_BOOTUP, config_msg, config_len);
        } else {
            ESP_LOGE(TAG, "Failed to build device config message");
        }
    } else {
        ESP_LOGE(TAG, "Failed to read device config from NVS");
    }

    ESP_LOGI(TAG, "Subscribed successful, msg_id=%d", msg_id);
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
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        handle_mqtt_connected(client);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
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
    memset(Previous_weather_message, 0, sizeof(Previous_weather_message));
    Flag_Active_Server = 0;
    mqtt_app_start();
}

void Mqtt__Subscribe(char *topic) {
    int msg_id = esp_mqtt_client_subscribe(client, topic, 0);
    ESP_LOGI(TAG, "Subscribed successful, msg_id=%d", msg_id);

}

void Mqtt__Publish(char *topic, const uint8_t *data, uint16_t data_len) {
    esp_mqtt_client_publish(client, topic, (const char *)data, data_len, 1, 0);
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
    // Load client certificate and key from NVS
    size_t cert_size = 0, key_size = 0;
    
    // Get certificate size and allocate buffer
    if (Device_Config__Get_Client_Cert(NULL, 0, &cert_size) != 0 || cert_size == 0) {
        ESP_LOGE(TAG, "Failed to get client certificate size from NVS");
        return;
    }
    
    client_cert_buffer = malloc(cert_size);
    if (client_cert_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for client certificate");
        return;
    }
    
    if (Device_Config__Get_Client_Cert(client_cert_buffer, cert_size, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to load client certificate from NVS");
        free(client_cert_buffer);
        client_cert_buffer = NULL;
        return;
    }
    
    // Get key size and allocate buffer
    if (Device_Config__Get_Client_Key(NULL, 0, &key_size) != 0 || key_size == 0) {
        ESP_LOGE(TAG, "Failed to get client key size from NVS");
        free(client_cert_buffer);
        client_cert_buffer = NULL;
        return;
    }
    
    client_key_buffer = malloc(key_size);
    if (client_key_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for client key");
        free(client_cert_buffer);
        client_cert_buffer = NULL;
        return;
    }
    
    if (Device_Config__Get_Client_Key(client_key_buffer, key_size, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to load client key from NVS");
        free(client_cert_buffer);
        free(client_key_buffer);
        client_cert_buffer = NULL;
        client_key_buffer = NULL;
        return;
    }
    
    ESP_LOGI(TAG, "Loaded client certificate (%d bytes) and key (%d bytes) from NVS", cert_size, key_size);
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
        // .broker.address.hostname = "jbar.dev",  // needed for device in home. 192.168.0.112 does not resolve
        .broker.verification.certificate = (const char *)ca_crt_start,
        .broker.verification.skip_cert_common_name_check = true,
        .credentials.authentication.certificate = client_cert_buffer,
        .credentials.authentication.key = client_key_buffer,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

// Handle incoming MQTT binary message
static void incoming_mqtt_handler(esp_mqtt_event_handle_t event) {
    // Set flag indicates receiving communications from server
    Flag_Active_Server = 1;

    // Create null-terminated topic string for comparison
    char topic_str[event->topic_len + 1];
    strncpy(topic_str, event->topic, event->topic_len);
    topic_str[event->topic_len] = '\0';
    
    ESP_LOGI(TAG, "Received %d bytes on topic: %s", event->data_len, topic_str);
    
    // Parse message header
    mqtt_msg_header_t header;
    if (mqtt_protocol_parse_header((uint8_t *)event->data, event->data_len, &header) != 0) {
        ESP_LOGE(TAG, "Failed to parse message header");
        return;
    }
    
    ESP_LOGI(TAG, "Parsed header: type=0x%02X, length=%d", header.type, header.length);
    
    const uint8_t *payload = (uint8_t *)event->data + MQTT_PROTOCOL_HEADER_SIZE;
    
    // Topic string applies to weather view of this device
    ESP_LOGI(TAG, "Topic comparison: received='%s' vs expected='%s'", topic_str, weather_topic_with_zip);
    if(strcmp(topic_str, weather_topic_with_zip) == 0) {
        ESP_LOGI(TAG, "Message on WEATHER_UPDATE topic");
        
        // Check if message is different from previous (avoid redundant processing)
        uint8_t is_duplicate = (memcmp(event->data, Previous_weather_message, event->data_len) == 0);
        ESP_LOGI(TAG, "Duplicate check: is_duplicate=%d, event->data_len=%d, buffer_size=%d", 
                 is_duplicate, event->data_len, sizeof(Previous_weather_message));
        
        if (!is_duplicate) {
            // Process based on message type
            ESP_LOGI(TAG, "Processing message type 0x%02X", header.type);
            switch (header.type) {
                case MSG_TYPE_CURRENT_WEATHER:
                    ESP_LOGI(TAG, "Processing CURRENT_WEATHER");
                    process_current_weather(payload, header.length);
                    break;
                    
                case MSG_TYPE_FORECAST_WEATHER:
                    ESP_LOGI(TAG, "Processing FORECAST_WEATHER");
                    process_forecast_weather(payload, header.length);
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown message type: 0x%02X", header.type);
                    break;
            }
            
            // Store message to detect duplicates
            if (event->data_len <= sizeof(Previous_weather_message)) {
                memcpy(Previous_weather_message, event->data, event->data_len);
            }
        }
    }
    else if(strcmp(topic_str, MQTT_TOPIC_DEV_SPECIFIC) == 0) {
        // Process version message
        if (header.type == MSG_TYPE_VERSION) {
            mqtt_version_t version;
            if (mqtt_protocol_parse_version(payload, header.length, &version) == 0) {
                check_and_trigger_ota_update(version.version);
            }
        } else {
            ESP_LOGW(TAG, "Unknown device-specific message type: 0x%02X", header.type);
        }
    }
}


// Check if server has newer version and trigger OTA update if needed
static void check_and_trigger_ota_update(uint8_t server_version) {
    ESP_LOGI(TAG, "Server version: %d, Device version: %d", server_version, FW_VERSION_NUM);
    
    if (server_version > FW_VERSION_NUM) {
        ESP_LOGI(TAG, "New firmware available (server=%d > device=%d), triggering OTA update", 
                 server_version, FW_VERSION_NUM);
        // Trigger OTA
        xSemaphoreGive(startOTASemaphore);
    }
}

// Process current weather message
static void process_current_weather(const uint8_t *payload, uint8_t payload_len) {
    mqtt_current_weather_t weather;
    
    if (mqtt_protocol_parse_current_weather(payload, payload_len, &weather) != 0) {
        ESP_LOGE(TAG, "Failed to parse current weather");
        return;
    }
    
    // Get actual temperature (with offset removed)
    int8_t actual_temp = mqtt_protocol_get_actual_temp(weather.temperature);
    
    // Prepare payload for weather module (API 0 = current weather)
    uint8_t weather_payload[1];
    weather_payload[0] = (uint8_t)actual_temp;  // Weather module expects actual temp
    
    ESP_LOGI(TAG, "Updating current weather: %d°F", actual_temp);
    Weather__Update_values(0, weather_payload, 1);
}

// Process forecast weather message
static void process_forecast_weather(const uint8_t *payload, uint8_t payload_len) {
    mqtt_forecast_weather_t forecast;
    
    if (mqtt_protocol_parse_forecast_weather(payload, payload_len, &forecast) != 0) {
        ESP_LOGE(TAG, "Failed to parse forecast weather");
        return;
    }
    
    // Prepare payload for weather module (API 1 = forecast)
    // Format: [num_days][day1_high][day1_precip][day1_moon][day2_high]...
    uint8_t weather_payload[1 + (7 * 3)];  // Max 7 days
    weather_payload[0] = forecast.num_days;
    
    uint8_t idx = 1;
    for (uint8_t i = 0; i < forecast.num_days; i++) {
        weather_payload[idx++] = forecast.days[i].high_temp;
        weather_payload[idx++] = forecast.days[i].precip;
        weather_payload[idx++] = forecast.days[i].moon;
    }
    
    ESP_LOGI(TAG, "Updating %d-day forecast", forecast.num_days);
    Weather__Update_values(1, weather_payload, idx);
}