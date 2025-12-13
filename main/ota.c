#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"

#include "ota.h"

static const char *TAG = "ota";

#define OTA_URL "https://jbar.dev/firmware.bin"

extern const uint8_t _binary_ca_crt_start[];
extern const uint8_t _binary_ca_crt_end[];

// Semaphore for triggering OTA from external modules
SemaphoreHandle_t startOTASemaphore = NULL;               

// Thread variables
TaskHandle_t blockingTaskHandle_OTA = NULL;

// Private method prototypes
static void download_image(void);
static char* get_pem_from_cert(void);
static void blocking_thread_start_OTA(void *pvParameters);

// Private method definitions

char* get_pem_from_cert(void) {
    char* ret_val = NULL;
    const uint8_t *crt_start = _binary_ca_crt_start;
    const uint8_t *crt_end = _binary_ca_crt_end;
    size_t cert_len = 0;
    char *cert_pem = NULL;

    if (crt_end > crt_start) {
        cert_len = (size_t)(crt_end - crt_start);
    }

    if (cert_len > 0 && cert_len < 65536) {
        cert_pem = malloc(cert_len + 1);
        if (cert_pem) {
            memcpy(cert_pem, crt_start, cert_len);
            cert_pem[cert_len] = '\0';
            ret_val = cert_pem;
        } else {
            ESP_LOGW(TAG, "Failed to allocate memory for cert, continuing without cert verification");
        }
    } else if (cert_len != 0) {
        ESP_LOGW(TAG, "Unexpected cert size %u, skipping cert", (unsigned)cert_len);
    } else {
        ESP_LOGW(TAG, "No embedded CA certificate found, skipping cert verification");
    }

    return(ret_val);
}

 void download_image(void) {
    ESP_LOGI(TAG, "Starting OTA download");

    char* cert_pem = get_pem_from_cert();
    if (cert_pem) {
        ESP_LOGI(TAG, "Using embedded CA certificate for TLS verification");
    } else {
        ESP_LOGW(TAG, "No CA certificate available - cert verification disabled");
    }

    esp_http_client_config_t my_http_config = {
        .url = OTA_URL,
        .cert_pem = cert_pem,
        .keep_alive_enable = false,                         // disable keep-alive to free connection resources
        .buffer_size = 4096,                                // 4KB buffer for better transfer reliability
        .skip_cert_common_name_check = true,                // for jbar.dev with self-signed cert
        .timeout_ms = 30000,                                // 30 second timeout for large firmware transfers
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &my_http_config,
        .bulk_flash_erase = false,  // Erase as we go instead of all at once (saves RAM)
    };

    esp_err_t ret = esp_https_ota(&ota_config);

    if (cert_pem) {
        free(cert_pem);
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA successful, restarting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA failed with error code 0x%x", ret);
    }
}

void blocking_thread_start_OTA(void *pvParameters) {
    while(1) {
        // Wait for semaphore from button press
        if(xSemaphoreTake(startOTASemaphore, portMAX_DELAY) == pdTRUE) {
            // Start OTA process: get image from jbar.dev
            download_image();
        }
    }
}

void OTA__Init(void) {
    ESP_LOGI(TAG, "OTA Init start");

    startOTASemaphore = xSemaphoreCreateBinary();
    if (startOTASemaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create OTA semaphore");
        return;
    }

    xTaskCreate(
        blocking_thread_start_OTA,       // Task function
        "BlockingTask_StartOTA",       // Task name (for debugging)
        8192,   // Stack size (words)
        NULL,                           // Task parameter
        8,                             // Task priority
        &blockingTaskHandle_OTA       // Task handle
    );
}
