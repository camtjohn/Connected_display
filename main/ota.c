#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "ota.h"
#include "device_config.h"

static const char *TAG = "ota";

// Semaphore for triggering OTA updates
SemaphoreHandle_t startOTASemaphore = NULL;

// NVS keys for factory boot control
#define NVS_NAMESPACE "boot_ctrl"
#define KEY_BOOT_TO_FACTORY "boot_factory"

// OTA server URL - MUST be HTTPS with TLS 1.2+
#define OTA_URL_MAX_LEN 256
#define DEFAULT_OTA_URL "https://jbar.dev/firmware.bin"
static char ota_url[OTA_URL_MAX_LEN] = DEFAULT_OTA_URL;

// Task handle for OTA download
static TaskHandle_t otaTaskHandle = NULL;

// Forward declarations
static void ota_download_task(void *pvParameters);

// Request boot to factory app for OTA update
void OTA__Request_Factory_Boot(void) {
    ESP_LOGI(TAG, "Setting flag to boot to factory app for OTA update");
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        uint8_t boot_flag = 1;
        nvs_set_u8(nvs_handle, KEY_BOOT_TO_FACTORY, boot_flag);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        
        ESP_LOGI(TAG, "Rebooting to factory app...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for factory boot flag: %s", esp_err_to_name(err));
    }
}

void OTA__Init(void) {
    ESP_LOGI(TAG, "OTA Init - checking boot state");
    
    // Create OTA trigger semaphore
    if (startOTASemaphore == NULL) {
        startOTASemaphore = xSemaphoreCreateBinary();
        if (startOTASemaphore == NULL) {
            ESP_LOGE(TAG, "Failed to create OTA semaphore");
            return;
        }
    }
    
    // Check if we just booted from an OTA update
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // First boot after OTA - mark as valid to prevent rollback
            ESP_LOGI(TAG, "First boot after OTA update - marking app as valid");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }
    
    ESP_LOGI(TAG, "Running from partition: %s", running->label);
    
    // Create OTA download task that waits for trigger semaphore
    xTaskCreate(
        ota_download_task,              // Task function
        "OTA_Download",                 // Task name
        (4*configMINIMAL_STACK_SIZE),   // Stack size (needs more for HTTPS)
        NULL,                           // Task parameter
        4,                              // Task priority (lower than MQTT)
        &otaTaskHandle                  // Task handle
    );
}

// OTA download task - waits for semaphore and performs HTTPS OTA update
static void ota_download_task(void *pvParameters) {
    while (1) {
        // Wait for OTA trigger signal from MQTT version message
        if (xSemaphoreTake(startOTASemaphore, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "OTA trigger received, starting HTTPS download");
            
            // Use hardcoded URL (can be overridden via OTA__Set_URL)
            if (strlen(ota_url) == 0) {
                ESP_LOGE(TAG, "OTA URL not set - cannot proceed with update");
                continue;
            }
            
            // Load CA certificate from NVS (provisioned)
            size_t ca_size = 0;
            if (Device_Config__Get_CA_Cert(NULL, 0, &ca_size) != 0 || ca_size == 0) {
                ESP_LOGE(TAG, "CA certificate not found in NVS (size=%d)", (int)ca_size);
                continue;
            }

            char *ca_cert = malloc(ca_size + 1);
            if (!ca_cert) {
                ESP_LOGE(TAG, "Failed to allocate %d bytes for CA cert", (int)ca_size);
                continue;
            }
            if (Device_Config__Get_CA_Cert(ca_cert, ca_size, &ca_size) != 0) {
                ESP_LOGE(TAG, "Failed to read CA cert from NVS");
                free(ca_cert);
                continue;
            }
            ca_cert[ca_size] = '\0'; // ensure null-terminated PEM

            // Configure HTTP client for HTTPS OTA with provisioned CA cert
            esp_http_client_config_t http_client_config = {
                .url = ota_url,
                .transport_type = HTTP_TRANSPORT_OVER_SSL,
                .disable_auto_redirect = false,
                .max_redirection_count = 10,
                .cert_pem = ca_cert,
                .cert_len = ca_size + 1,
            };
            
            // Configure HTTPS OTA
            esp_https_ota_config_t ota_config = {
                .http_config = &http_client_config,
                .bulk_flash_erase = false,
                .partial_http_download = true,
            };
            
            ESP_LOGI(TAG, "Starting HTTPS OTA from: %s", ota_url);
            esp_err_t ret = esp_https_ota(&ota_config);
            
            free(ca_cert);

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "OTA update successful - rebooting");
                vTaskDelay(pdMS_TO_TICKS(2000));
                esp_restart();
            } else {
                ESP_LOGE(TAG, "OTA update failed: %s (%d)", esp_err_to_name(ret), ret);
                // Log SSL/TLS connection errors
                if (ret == ESP_FAIL) {
                    ESP_LOGE(TAG, "OTA connection failed - check URL, TLS version, and server certificate");
                }
            }
        }
    }
}
