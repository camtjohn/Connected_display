#ifndef PROVISIONING_H
#define PROVISIONING_H

#include <stddef.h>

// WiFi credential limits (from wifi_config_t structure)
#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASS_MAX_LEN 64

/**
 * @brief Start WiFi provisioning
 * 
 * Starts SoftAP mode on "Connected_Display" and hosts an HTTP server
 * for users to enter WiFi credentials. Blocks until provisioning is complete.
 * 
 * @return 0 on success, -1 on failure
 */
int Provisioning__Start(void);

/**
 * @brief Stop WiFi provisioning
 * 
 * Stops the HTTP server and SoftAP mode, cleans up resources
 */
void Provisioning__Stop(void);

/**
 * @brief Check if provisioning is currently active
 * 
 * @return 1 if active, 0 if inactive
 */
int Provisioning__Is_Active(void);

#endif // PROVISIONING_H
