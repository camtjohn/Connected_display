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

#endif // DEVICE_CONFIG_H
