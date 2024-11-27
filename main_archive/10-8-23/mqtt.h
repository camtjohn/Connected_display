#ifndef MQTT_H
#define MQTT_H

#define MQTT_BROKER_URL "mqtt://10.0.0.110"
#define MQTT_TOPIC_TO_THERM "ferm_to_therm"
#define MQTT_TOPIC_FROM_THERM "ferm_from_therm"
#define MQTT_TOPIC_FROM_THERM_NOTIFY "ferm_notify_temp"

void start_mqtt(void);
void pub_mqtt(char*, char*);

#endif