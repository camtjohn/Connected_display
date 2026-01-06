#ifndef WIFI_H
#define WIFI_H

#define INIT_WIFI_MAXIMUM_RETRY  3

// Start WiFi connection. Returns 0 on success, -1 on failure
int Wifi__Start(void);

#endif