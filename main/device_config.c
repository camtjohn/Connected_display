#include "device_config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <string.h>

static const char *TAG = "DEVICE_CONFIG";
static const char *NVS_NAMESPACE = "device_cfg";

esp_err_t DeviceConfig_Init(void) {
    esp_err_t ret = nvs_flash_init();
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "NVS initialized successfully");
    } else {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t DeviceConfig_Load(device_config_t *config) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, using defaults");
        // Set defaults
        strcpy(config->device_name, DEFAULT_DEVICE_NUMBER);
        strcpy(config->zip_code, DEFAULT_ZIP_CODE);
        config->brightness = DEFAULT_BRIGHT;
        config->timezone_offset = DEFAULT_TIMEZONE;
        return ESP_ERR_NVS_NOT_FOUND;
    }

    size_t required_size;
    
    // Read device name
    required_size = sizeof(config->device_name);
    err = nvs_get_str(nvs_handle, "device_num", config->device_name, &required_size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Device name not found in NVS, using default");
        strcpy(config->device_name, DEFAULT_DEVICE_NUMBER);
    }

    // Read zip code
    required_size = sizeof(config->zip_code);
    err = nvs_get_str(nvs_handle, "zip_code", config->zip_code, &required_size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Zip code not found in NVS, using default");
        strcpy(config->zip_code, DEFAULT_ZIP_CODE);
    }

    // Read brightness
    err = nvs_get_u8(nvs_handle, "brightness", &config->brightness);
    if (err != ESP_OK) {
        config->brightness = 128;
    }

    // Read timezone offset
    err = nvs_get_u8(nvs_handle, "tz_offset", &config->timezone_offset);
    if (err != ESP_OK) {
        config->timezone_offset = 0;
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

esp_err_t DeviceConfig_Save(const device_config_t *config) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return err;
    }

    // Save device name
    err = nvs_set_str(nvs_handle, "device_num", config->device_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save device name: %s", esp_err_to_name(err));
        goto cleanup;
    }

    // Save zip code
    err = nvs_set_str(nvs_handle, "zip_code", config->zip_code);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save zip code: %s", esp_err_to_name(err));
        goto cleanup;
    }

    // Save brightness
    err = nvs_set_u8(nvs_handle, "brightness", config->brightness);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save brightness: %s", esp_err_to_name(err));
        goto cleanup;
    }

    // Save timezone offset
    err = nvs_set_u8(nvs_handle, "tz_offset", config->timezone_offset);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save timezone offset: %s", esp_err_to_name(err));
        goto cleanup;
    }

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(err));
    }
    
cleanup:
    nvs_close(nvs_handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Config saved successfully");
    }
    return err;
}

esp_err_t DeviceConfig_GetDeviceName(char *buffer, size_t buffer_size) {
    device_config_t config;
    esp_err_t err = DeviceConfig_Load(&config);
    if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
        strncpy(buffer, config.device_name, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return ESP_OK;
    }
    return err;
}

esp_err_t DeviceConfig_GetZipCode(char *buffer, size_t buffer_size) {
    device_config_t config;
    esp_err_t err = DeviceConfig_Load(&config);
    if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
        strncpy(buffer, config.zip_code, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return ESP_OK;
    }
    return err;
}

esp_err_t DeviceConfig_SetDeviceName(const char *device_name) {
    device_config_t config;
    DeviceConfig_Load(&config);  // Load current config (ignore errors, use defaults)
    strncpy(config.device_name, device_name, sizeof(config.device_name) - 1);
    config.device_name[sizeof(config.device_name) - 1] = '\0';
    return DeviceConfig_Save(&config);
}

esp_err_t DeviceConfig_SetZipCode(const char *zip) {
    device_config_t config;
    DeviceConfig_Load(&config);  // Load current config (ignore errors, use defaults)
    strncpy(config.zip_code, zip, sizeof(config.zip_code) - 1);
    config.zip_code[sizeof(config.zip_code) - 1] = '\0';
    return DeviceConfig_Save(&config);
}

esp_err_t DeviceConfig_GetWiFiCredentials(char *ssid, size_t ssid_size, char *password, size_t password_size) {
    wifi_config_t wifi_config;
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get WiFi config: %s", esp_err_to_name(err));
        ssid[0] = '\0';
        password[0] = '\0';
        return err;
    }
    
    strncpy(ssid, (char*)wifi_config.sta.ssid, ssid_size - 1);
    ssid[ssid_size - 1] = '\0';
    strncpy(password, (char*)wifi_config.sta.password, password_size - 1);
    password[password_size - 1] = '\0';
    
    return ESP_OK;
}

esp_err_t DeviceConfig_SetWiFiCredentials(const char *ssid, const char *password) {
    wifi_config_t wifi_config = {0};
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "WiFi credentials updated: SSID=%s", ssid);
    ESP_LOGI(TAG, "Reconnecting to WiFi...");
    
    // Trigger reconnection with new credentials
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_wifi_connect();
    
    return ESP_OK;
}
