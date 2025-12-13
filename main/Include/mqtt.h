#ifndef MQTT_H
#define MQTT_H

// Bad implementation of allowing mqtt module access task to pause/resume while updating display
extern TaskHandle_t periodicTaskHandle;
extern uint16_t view_main[16];

// This version number
#define FW_VERSION_NUM                  1
#define DEVICE_NAME                     "dev0"

#define MQTT_BROKER_URL                 "mqtts://jbar.dev:8883"
// #define MQTT_BROKER_URL             "mqtts://192.168.0.112:8883"

// Debug build flag - uncomment to use debug topics
#define DEBUG_BUILD

#ifdef DEBUG_BUILD
    // Debug topics for testing
    #define MQTT_TOPIC_WEATHER_UPDATE       "debug_weather_60607"
    #define MQTT_TOPIC_DEV_SPECIFIC         "debug_dev0"
    #define MQTT_TOPIC_BOOTUP               "debug_dev_bootup"
    #define MQTT_TOPIC_HEARTBEAT            "debug_dev_heartbeat"
    #define MQTT_TOPIC_OFFLINE              "debug_device_offline"
    #define MQTT_TOPIC_TEST                 "debug_test_msg"
#else
    // Production topics
    #define MQTT_TOPIC_WEATHER_UPDATE       "weather_60607"
    #define MQTT_TOPIC_DEV_SPECIFIC         DEVICE_NAME
    #define MQTT_TOPIC_BOOTUP               "dev_bootup"
    #define MQTT_TOPIC_HEARTBEAT            "dev_heartbeat"
    #define MQTT_TOPIC_OFFLINE              "device_offline"
#endif

// This client publishes to these topics (notify/update server)
#define MQTT_TOPIC_SUB_NOTIFY           "subsribe_from_0"
#define MQTT_TOPIC_DATA_TO_SERVER       "data_from_0"

void Mqtt__Start(void);
void Mqtt__Send_debug_msg(char *);
uint8_t Mqtt__Get_server_status(void);
void Mqtt__Reset_server_status(void);
// Private move to mqtt.c?
void Mqtt__Subscribe(char*);
void Mqtt__Publish(char *, char *);


#endif