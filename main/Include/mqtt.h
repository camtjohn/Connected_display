#ifndef MQTT_H
#define MQTT_H

#include <stdbool.h>

// Bad implementation of allowing mqtt module access task to pause/resume while updating display
extern TaskHandle_t periodicTaskHandle;
extern uint16_t view_main[16];

// Debug build flag - uncomment to use debug topics
// #define DEBUG_BUILD

#define MQTT_BROKER_URL                 "mqtts://jbar.dev:8883"

#ifdef DEBUG_BUILD
    // Debug topics for testing (zipcode appended at runtime)
    #define MQTT_TOPIC_WEATHER_BASE         "debug_weather"
    #define MQTT_TOPIC_DEV_SPECIFIC         "debug_dev0"
    #define MQTT_TOPIC_BOOTUP               "debug_dev_bootup"
    #define MQTT_TOPIC_HEARTBEAT            "debug_dev_heartbeat"
    #define MQTT_TOPIC_OFFLINE              "debug_device_offline"
    #define MQTT_TOPIC_TEST                 "debug_test_msg"
    #define MQTT_TOPIC_SHARED_VIEW          "debug_shared_view" // Added shared view topic for debug
#else
    // Production topics (zipcode appended at runtime)
    #define MQTT_TOPIC_WEATHER_BASE         "weather"
    #define MQTT_TOPIC_DEV_SPECIFIC         DEVICE_NAME
    #define MQTT_TOPIC_BOOTUP               "dev_bootup"
    #define MQTT_TOPIC_HEARTBEAT            "dev_heartbeat"
    #define MQTT_TOPIC_OFFLINE              "device_offline"
    #define MQTT_TOPIC_SHARED_VIEW          "shared_view" // Added shared view topic for production
#endif

// This client publishes to these topics (notify/update server)
#define MQTT_TOPIC_SUB_NOTIFY           "subsribe_from_0"
#define MQTT_TOPIC_DATA_TO_SERVER       "data_from_0"

void Mqtt__Start(void);
void Mqtt__Send_debug_msg(char *);
uint8_t Mqtt__Get_server_status(void);
void Mqtt__Reset_server_status(void);
bool Mqtt__Is_connected(void);
bool Mqtt__Is_offline(void);
void Mqtt__Set_offline_mode(bool offline);
// Private move to mqtt.c?
void Mqtt__Subscribe(char*);
void Mqtt__Publish(char *topic, const uint8_t *data, uint16_t data_len);


#endif