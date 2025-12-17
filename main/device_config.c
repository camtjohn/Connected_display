#include "device_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "DEVICE_CONFIG";
static const char *NVS_NAMESPACE = "device_cfg";

// In-memory cache to avoid excessive NVS reads
static struct {
    char device_name[16];
    char zipcode[8];
    bool initialized;
} config_cache = {
    .device_name = "dev0",
    .zipcode = "60607",
    .initialized = false
};

int Device_Config__Init(void) {
    esp_err_t err;
    nvs_handle_t nvs_handle;
    
    // Initialize NVS if not already done
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Open NVS
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Read device name (or use default)
    size_t name_len = sizeof(config_cache.device_name);
    err = nvs_get_str(nvs_handle, "name", config_cache.device_name, &name_len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Device name not found in NVS, using default: %s", config_cache.device_name);
        // Write default to NVS
        nvs_set_str(nvs_handle, "name", config_cache.device_name);
    } else if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error reading device name: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Loaded device name from NVS: %s", config_cache.device_name);
    }
    
    // Read zipcode (or use default)
    size_t zip_len = sizeof(config_cache.zipcode);
    err = nvs_get_str(nvs_handle, "zipcode", config_cache.zipcode, &zip_len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Zipcode not found in NVS, using default: %s", config_cache.zipcode);
        nvs_set_str(nvs_handle, "zipcode", config_cache.zipcode);
    } else if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error reading zipcode: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Loaded zipcode from NVS: %s", config_cache.zipcode);
    }
    
    // Commit writes
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    config_cache.initialized = true;
    ESP_LOGI(TAG, "Device config initialized: %s, %s", 
             config_cache.device_name, config_cache.zipcode);
    
    return 0;
}

int Device_Config__Get_Name(char *name, size_t max_len) {
    if (name == NULL || max_len == 0) {
        return -1;
    }
    
    if (!config_cache.initialized) {
        ESP_LOGW(TAG, "Config not initialized, call Device_Config__Init() first");
        return -1;
    }
    
    strncpy(name, config_cache.device_name, max_len - 1);
    name[max_len - 1] = '\0';
    return 0;
}

int Device_Config__Get_Zipcode(char *zipcode, size_t max_len) {
    if (zipcode == NULL || max_len == 0) {
        return -1;
    }
    
    if (!config_cache.initialized) {
        ESP_LOGW(TAG, "Config not initialized, call Device_Config__Init() first");
        return -1;
    }
    
    strncpy(zipcode, config_cache.zipcode, max_len - 1);
    zipcode[max_len - 1] = '\0';
    return 0;
}


int Device_Config__Set_Name(const char *name) {
    if (name == NULL) {
        return -1;
    }
    
    // Only write if different (avoid unnecessary NVS writes)
    if (strcmp(config_cache.device_name, name) == 0) {
        ESP_LOGD(TAG, "Device name unchanged, skipping write");
        return 0;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return -1;
    }
    
    err = nvs_set_str(nvs_handle, "name", name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write name: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }
    
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    strncpy(config_cache.device_name, name, sizeof(config_cache.device_name) - 1);
    config_cache.device_name[sizeof(config_cache.device_name) - 1] = '\0';
    
    ESP_LOGI(TAG, "Device name updated: %s", name);
    return 0;
}

int Device_Config__Set_Zipcode(const char *zipcode) {
    if (zipcode == NULL) {
        return -1;
    }
    
    // Only write if different
    if (strcmp(config_cache.zipcode, zipcode) == 0) {
        ESP_LOGD(TAG, "Zipcode unchanged, skipping write");
        return 0;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return -1;
    }
    
    err = nvs_set_str(nvs_handle, "zipcode", zipcode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write zipcode: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }
    
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    strncpy(config_cache.zipcode, zipcode, sizeof(config_cache.zipcode) - 1);
    config_cache.zipcode[sizeof(config_cache.zipcode) - 1] = '\0';
    
    ESP_LOGI(TAG, "Zipcode updated: %s", zipcode);
    return 0;
}
