#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Initialize device configuration storage
 * Loads config from NVS or sets defaults
 * 
 * @return 0 on success, -1 on error
 */
int Device_Config__Init(void);

/**
 * @brief Get device name
 * 
 * @param name Buffer to store device name (min 16 bytes)
 * @param max_len Maximum length of buffer
 * @return 0 on success, -1 on error
 */
int Device_Config__Get_Name(char *name, size_t max_len);

/**
 * @brief Get zipcode
 * 
 * @param zipcode Buffer to store zipcode (min 8 bytes)
 * @param max_len Maximum length of buffer
 * @return 0 on success, -1 on error
 */
int Device_Config__Get_Zipcode(char *zipcode, size_t max_len);

/**
 * @brief Get user name
 * 
 * @param user_name Buffer to store user name (min 16 bytes)
 * @param max_len Maximum length of buffer
 * @return 0 on success, -1 on error
 */
int Device_Config__Get_UserName(char *user_name, size_t max_len);

/**
 * @brief Set device name (writes to NVS)
 * 
 * @param name Device name to store
 * @return 0 on success, -1 on error
 */
int Device_Config__Set_Name(const char *name);

/**
 * @brief Set zipcode (writes to NVS)
 * 
 * @param zipcode Zipcode to store
 * @return 0 on success, -1 on error
 */
int Device_Config__Set_Zipcode(const char *zipcode);

/**
 * @brief Set user name (writes to NVS)
 * 
 * @param user_name User name to store
 * @return 0 on success, -1 on error
 */
int Device_Config__Set_UserName(const char *user_name);

/**
 * @brief Get client TLS certificate from NVS
 * 
 * @param cert_buffer Buffer to store certificate (allocated by caller)
 * @param buffer_size Size of buffer
 * @param actual_size Pointer to store actual certificate size
 * @return 0 on success, -1 on error
 */
int Device_Config__Get_Client_Cert(char *cert_buffer, size_t buffer_size, size_t *actual_size);

/**
 * @brief Get client TLS key from NVS
 * 
 * @param key_buffer Buffer to store key (allocated by caller)
 * @param buffer_size Size of buffer
 * @param actual_size Pointer to store actual key size
 * @return 0 on success, -1 on error
 */
int Device_Config__Get_Client_Key(char *key_buffer, size_t buffer_size, size_t *actual_size);

/**
 * @brief Set client TLS certificate in NVS
 * 
 * @param cert Certificate string (null-terminated)
 * @param cert_len Length of certificate
 * @return 0 on success, -1 on error
 */
int Device_Config__Set_Client_Cert(const char *cert, size_t cert_len);

/**
 * @brief Set client TLS key in NVS
 * 
 * @param key Key string (null-terminated)
 * @param key_len Length of key
 * @return 0 on success, -1 on error
 */
int Device_Config__Set_Client_Key(const char *key, size_t key_len);

/**
 * @brief Get CA certificate from NVS
 * 
 * @param ca_buffer Buffer to store certificate (allocated by caller)
 * @param buffer_size Size of buffer
 * @param actual_size Pointer to store actual certificate size
 * @return 0 on success, -1 on error
 */
int Device_Config__Get_CA_Cert(char *ca_buffer, size_t buffer_size, size_t *actual_size);

/**
 * @brief Set CA certificate in NVS
 * 
 * @param cert Certificate string (null-terminated)
 * @param cert_len Length of certificate
 * @return 0 on success, -1 on error
 */
int Device_Config__Set_CA_Cert(const char *cert, size_t cert_len);

/**
 * @brief Get WiFi SSID
 * 
 * @param ssid Buffer to store SSID (min 32 bytes)
 * @param max_len Maximum length of buffer
 * @return 0 on success, -1 on error
 */
int Device_Config__Get_WiFi_SSID(char *ssid, size_t max_len);

/**
 * @brief Get WiFi Password
 * 
 * @param password Buffer to store password (min 64 bytes)
 * @param max_len Maximum length of buffer
 * @return 0 on success, -1 on error
 */
int Device_Config__Get_WiFi_Password(char *password, size_t max_len);

/**
 * @brief Set WiFi SSID (writes to NVS)
 * 
 * @param ssid WiFi network name to store
 * @return 0 on success, -1 on error
 */
int Device_Config__Set_WiFi_SSID(const char *ssid);

/**
 * @brief Set WiFi Password (writes to NVS)
 * 
 * @param password WiFi password to store
 * @return 0 on success, -1 on error
 */
int Device_Config__Set_WiFi_Password(const char *password);

#endif // DEVICE_CONFIG_H
