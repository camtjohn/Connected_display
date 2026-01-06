#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "ota.h"

static const char *TAG = "ota";

// NVS keys for factory boot control
#define NVS_NAMESPACE "boot_ctrl"
#define KEY_BOOT_TO_FACTORY "boot_factory"

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
}
