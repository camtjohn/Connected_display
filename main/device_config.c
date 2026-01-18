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
    char user_name[16];
    char wifi_ssid[32];
    char wifi_password[64];
    bool initialized;
} config_cache = {
    .device_name = "dev00",
    .zipcode = "60607",
    .user_name = "DefaultUser",
    .wifi_ssid = "",
    .wifi_password = "",
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
    
    // Read user name (or use default)
    size_t user_len = sizeof(config_cache.user_name);
    err = nvs_get_str(nvs_handle, "user_name", config_cache.user_name, &user_len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "User name not found in NVS, using default: %s", config_cache.user_name);
        nvs_set_str(nvs_handle, "user_name", config_cache.user_name);
    } else if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error reading user name: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Loaded user name from NVS: %s", config_cache.user_name);
    }
    
    // Read WiFi SSID (optional - may not be set)
    size_t ssid_len = sizeof(config_cache.wifi_ssid);
    err = nvs_get_str(nvs_handle, "wifi_ssid", config_cache.wifi_ssid, &ssid_len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "WiFi SSID not found in NVS");
        config_cache.wifi_ssid[0] = '\0';
    } else if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error reading WiFi SSID: %s", esp_err_to_name(err));
        config_cache.wifi_ssid[0] = '\0';
    } else {
        ESP_LOGI(TAG, "Loaded WiFi SSID from NVS: %s", config_cache.wifi_ssid);
    }
    
    // Read WiFi password (optional - may not be set)
    size_t pass_len = sizeof(config_cache.wifi_password);
    err = nvs_get_str(nvs_handle, "wifi_password", config_cache.wifi_password, &pass_len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "WiFi password not found in NVS");
        config_cache.wifi_password[0] = '\0';
    } else if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error reading WiFi password: %s", esp_err_to_name(err));
        config_cache.wifi_password[0] = '\0';
    } else {
        ESP_LOGI(TAG, "Loaded WiFi password from NVS");
    }
    
    // Commit writes
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    config_cache.initialized = true;
    ESP_LOGI(TAG, "Device config initialized: %s, %s, %s", 
             config_cache.device_name, config_cache.zipcode, config_cache.user_name);
    
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

int Device_Config__Get_UserName(char *user_name, size_t max_len) {
    if (user_name == NULL || max_len == 0) {
        return -1;
    }
    
    if (!config_cache.initialized) {
        ESP_LOGW(TAG, "Config not initialized, call Device_Config__Init() first");
        return -1;
    }
    
    strncpy(user_name, config_cache.user_name, max_len - 1);
    user_name[max_len - 1] = '\0';
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

int Device_Config__Set_UserName(const char *user_name) {
    if (user_name == NULL) {
        return -1;
    }
    
    // Only write if different
    if (strcmp(config_cache.user_name, user_name) == 0) {
        ESP_LOGD(TAG, "User name unchanged, skipping write");
        return 0;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return -1;
    }
    
    err = nvs_set_str(nvs_handle, "user_name", user_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write user name: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }
    
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    strncpy(config_cache.user_name, user_name, sizeof(config_cache.user_name) - 1);
    config_cache.user_name[sizeof(config_cache.user_name) - 1] = '\0';
    
    ESP_LOGI(TAG, "User name updated: %s", user_name);
    return 0;
}

int Device_Config__Get_Client_Cert(char *cert_buffer, size_t buffer_size, size_t *actual_size) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return -1;
    }
    
    size_t required_size = 0;
    err = nvs_get_blob(nvs_handle, "client_cert", NULL, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Client certificate not found in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }
    
    // If only querying size, return it
    if (cert_buffer == NULL || buffer_size == 0) {
        if (actual_size != NULL) {
            *actual_size = required_size;
        }
        nvs_close(nvs_handle);
        return 0;
    }
    
    if (required_size > buffer_size) {
        ESP_LOGE(TAG, "Buffer too small for certificate: %d > %d", required_size, buffer_size);
        nvs_close(nvs_handle);
        return -1;
    }
    
    err = nvs_get_blob(nvs_handle, "client_cert", cert_buffer, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read client certificate: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }
    
    if (actual_size != NULL) {
        *actual_size = required_size;
    }
    
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Loaded client certificate from NVS (%d bytes)", required_size);
    return 0;
}

int Device_Config__Get_Client_Key(char *key_buffer, size_t buffer_size, size_t *actual_size) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return -1;
    }
    
    size_t required_size = 0;
    err = nvs_get_blob(nvs_handle, "client_key", NULL, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Client key not found in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }
    
    // If only querying size, return it
    if (key_buffer == NULL || buffer_size == 0) {
        if (actual_size != NULL) {
            *actual_size = required_size;
        }
        nvs_close(nvs_handle);
        return 0;
    }
    
    if (required_size > buffer_size) {
        ESP_LOGE(TAG, "Buffer too small for key: %d > %d", required_size, buffer_size);
        nvs_close(nvs_handle);
        return -1;
    }
    
    err = nvs_get_blob(nvs_handle, "client_key", key_buffer, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read client key: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }
    
    if (actual_size != NULL) {
        *actual_size = required_size;
    }
    
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Loaded client key from NVS (%d bytes)", required_size);
    return 0;
}

int Device_Config__Set_Client_Cert(const char *cert, size_t cert_len) {
    if (cert == NULL || cert_len == 0) {
        return -1;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return -1;
    }
    
    err = nvs_set_blob(nvs_handle, "client_cert", cert, cert_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write client certificate: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }
    
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Client certificate saved to NVS (%d bytes)", cert_len);
    return 0;
}

int Device_Config__Set_Client_Key(const char *key, size_t key_len) {
    if (key == NULL || key_len == 0) {
        return -1;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return -1;
    }
    
    err = nvs_set_blob(nvs_handle, "client_key", key, key_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write client key: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }
    
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Client key saved to NVS (%d bytes)", key_len);
    return 0;
}

int Device_Config__Get_CA_Cert(char *ca_buffer, size_t buffer_size, size_t *actual_size) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return -1;
    }

    size_t required_size = 0;
    err = nvs_get_blob(nvs_handle, "ca_cert", NULL, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CA certificate not found in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }

    if (ca_buffer == NULL || buffer_size == 0) {
        if (actual_size != NULL) {
            *actual_size = required_size;
        }
        nvs_close(nvs_handle);
        return 0;
    }

    if (required_size > buffer_size) {
        ESP_LOGE(TAG, "Buffer too small for CA certificate: %d > %d", required_size, buffer_size);
        nvs_close(nvs_handle);
        return -1;
    }

    err = nvs_get_blob(nvs_handle, "ca_cert", ca_buffer, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read CA certificate: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }

    if (actual_size != NULL) {
        *actual_size = required_size;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Loaded CA certificate from NVS (%d bytes)", required_size);
    return 0;
}

int Device_Config__Set_CA_Cert(const char *cert, size_t cert_len) {
    if (cert == NULL || cert_len == 0) {
        return -1;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return -1;
    }

    err = nvs_set_blob(nvs_handle, "ca_cert", cert, cert_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write CA certificate: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }

    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "CA certificate saved to NVS (%d bytes)", cert_len);
    return 0;
}

int Device_Config__Get_WiFi_SSID(char *ssid, size_t max_len) {
    if (ssid == NULL || max_len == 0) {
        return -1;
    }
    
    if (!config_cache.initialized) {
        ESP_LOGW(TAG, "Config not initialized, call Device_Config__Init() first");
        return -1;
    }
    
    strncpy(ssid, config_cache.wifi_ssid, max_len - 1);
    ssid[max_len - 1] = '\0';
    return 0;
}

int Device_Config__Get_WiFi_Password(char *password, size_t max_len) {
    if (password == NULL || max_len == 0) {
        return -1;
    }
    
    if (!config_cache.initialized) {
        ESP_LOGW(TAG, "Config not initialized, call Device_Config__Init() first");
        return -1;
    }
    
    strncpy(password, config_cache.wifi_password, max_len - 1);
    password[max_len - 1] = '\0';
    return 0;
}

int Device_Config__Set_WiFi_SSID(const char *ssid) {
    if (ssid == NULL) {
        return -1;
    }
    
    // Only write if different
    if (strcmp(config_cache.wifi_ssid, ssid) == 0) {
        ESP_LOGD(TAG, "WiFi SSID unchanged, skipping write");
        return 0;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return -1;
    }
    
    err = nvs_set_str(nvs_handle, "wifi_ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write WiFi SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }
    
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    strncpy(config_cache.wifi_ssid, ssid, sizeof(config_cache.wifi_ssid) - 1);
    config_cache.wifi_ssid[sizeof(config_cache.wifi_ssid) - 1] = '\0';
    
    ESP_LOGI(TAG, "WiFi SSID updated: %s", ssid);
    return 0;
}

int Device_Config__Set_WiFi_Password(const char *password) {
    if (password == NULL) {
        return -1;
    }
    
    // Only write if different
    if (strcmp(config_cache.wifi_password, password) == 0) {
        ESP_LOGD(TAG, "WiFi password unchanged, skipping write");
        return 0;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return -1;
    }
    
    err = nvs_set_str(nvs_handle, "wifi_password", password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write WiFi password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }
    
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    strncpy(config_cache.wifi_password, password, sizeof(config_cache.wifi_password) - 1);
    config_cache.wifi_password[sizeof(config_cache.wifi_password) - 1] = '\0';
    
    ESP_LOGI(TAG, "WiFi password updated");
    return 0;
}
