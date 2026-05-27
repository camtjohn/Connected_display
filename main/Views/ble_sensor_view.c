#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "ble_sensor_view.h"
#include "view.h"
#include "mqtt.h"

#if CONFIG_BT_ENABLED
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#endif

static const char *TAG = "WEATHER_STATION: BLE_VIEW";

#define SENSOR_INTERVAL_MS            900000ULL
#define GUARD_WINDOW_MS               60000ULL
#define MAX_CONSECUTIVE_MISSES        3U
#define VIEW_REFRESH_MS               1000U

#define MFR_MAGIC_0                   0x88
#define MFR_MAGIC_1                   0x77
#define MFR_PAYLOAD_LEN               6

typedef enum {
    BLE_PHASE_ACQUISITION = 0,
    BLE_PHASE_LOCKED_WAIT,
    BLE_PHASE_LOCKED_SCAN,
} ble_phase_t;

#if CONFIG_BT_ENABLED
static portMUX_TYPE s_ble_lock = portMUX_INITIALIZER_UNLOCKED;

static TaskHandle_t s_ble_task_handle = NULL;
static ble_phase_t s_phase = BLE_PHASE_ACQUISITION;
static uint8_t s_have_sync = 0;
static uint8_t s_is_scanning = 0;
static uint8_t s_packet_seen_in_window = 0;
static uint8_t s_miss_count = 0;
static uint16_t s_sensor_id = 0;
static int16_t s_temperature = 0;
static int64_t s_last_rx_ms = 0;
static int64_t s_guard_scan_deadline_ms = 0;
static bool s_gap_ready = false;

static esp_ble_scan_params_t s_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30,
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
};

static int64_t now_ms(void) {
    return esp_timer_get_time() / 1000;
}

static void publish_temperature(uint16_t sensor_id, int16_t temperature) {
    if (!Mqtt__Is_connected()) {
        return;
    }

    char topic[64];
    char payload[16];

    int topic_len = snprintf(topic, sizeof(topic), "sensors/%u/temperature", (unsigned int)sensor_id);
    int payload_len = snprintf(payload, sizeof(payload), "%d", (int)temperature);

    if (topic_len <= 0 || topic_len >= (int)sizeof(topic) || payload_len <= 0 || payload_len >= (int)sizeof(payload)) {
        return;
    }

    Mqtt__Publish(topic, (const uint8_t *)payload, (uint16_t)payload_len);
}

static void handle_valid_packet(uint16_t sensor_id, int16_t temperature) {
    int64_t now = now_ms();

    taskENTER_CRITICAL(&s_ble_lock);
    s_have_sync = 1;
    s_sensor_id = sensor_id;
    s_temperature = temperature;
    s_last_rx_ms = now;
    s_miss_count = 0;
    if (s_phase == BLE_PHASE_LOCKED_SCAN) {
        s_packet_seen_in_window = 1;
    }
    s_phase = BLE_PHASE_LOCKED_WAIT;
    taskEXIT_CRITICAL(&s_ble_lock);

    publish_temperature(sensor_id, temperature);
    xSemaphoreGive(displayUpdateSemaphore);
}

static bool parse_and_handle_mfr_packet(const uint8_t *adv_data, uint8_t adv_len) {
    uint8_t mfr_len = 0;
    uint8_t *mfr = esp_ble_resolve_adv_data((uint8_t *)adv_data, ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE, &mfr_len);

    if (!mfr || mfr_len != MFR_PAYLOAD_LEN) {
        return false;
    }
    if (mfr[0] != MFR_MAGIC_0 || mfr[1] != MFR_MAGIC_1) {
        return false;
    }

    uint16_t sensor_id = (uint16_t)((mfr[2] << 8) | mfr[3]);
    int16_t temperature = (int16_t)((mfr[4] << 8) | mfr[5]);
    handle_valid_packet(sensor_id, temperature);
    (void)adv_len;
    return true;
}

static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
            s_gap_ready = true;
            break;

        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
            if (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                taskENTER_CRITICAL(&s_ble_lock);
                s_is_scanning = 1;
                taskEXIT_CRITICAL(&s_ble_lock);
            }
            break;

        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            taskENTER_CRITICAL(&s_ble_lock);
            s_is_scanning = 0;
            taskEXIT_CRITICAL(&s_ble_lock);
            break;

        case ESP_GAP_BLE_SCAN_RESULT_EVT:
            if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                parse_and_handle_mfr_packet(param->scan_rst.ble_adv, param->scan_rst.adv_data_len);
            }
            break;

        default:
            break;
    }
}

static void stop_scanning_if_needed(void) {
    uint8_t is_scanning = 0;

    taskENTER_CRITICAL(&s_ble_lock);
    is_scanning = s_is_scanning;
    taskEXIT_CRITICAL(&s_ble_lock);

    if (is_scanning) {
        esp_ble_gap_stop_scanning();
    }
}

static void start_continuous_scan(void) {
    uint8_t is_scanning = 0;

    taskENTER_CRITICAL(&s_ble_lock);
    is_scanning = s_is_scanning;
    taskEXIT_CRITICAL(&s_ble_lock);

    if (!is_scanning) {
        esp_ble_gap_start_scanning(0);
    }
}

static esp_err_t init_ble_stack(void) {
    esp_err_t err;

    err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    esp_bt_controller_status_t ctrl_status = esp_bt_controller_get_status();
    if (ctrl_status == ESP_BT_CONTROLLER_STATUS_IDLE) {
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        err = esp_bt_controller_init(&bt_cfg);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return err;
        }
    }

    ctrl_status = esp_bt_controller_get_status();
    if (ctrl_status == ESP_BT_CONTROLLER_STATUS_INITED) {
        err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return err;
        }
    }

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        err = esp_bluedroid_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return err;
        }
    }

    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        err = esp_bluedroid_enable();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return err;
        }
    }

    err = esp_ble_gap_register_callback(gap_cb);
    if (err != ESP_OK) {
        return err;
    }

    return esp_ble_gap_set_scan_params(&s_scan_params);
}

static void ble_scan_task(void *arg) {
    (void)arg;

    esp_err_t err = init_ble_stack();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BLE init failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    while (!s_gap_ready) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    for (;;) {
        ble_phase_t phase;
        uint8_t have_sync;
        int64_t last_rx_ms;
        int64_t guard_deadline_ms;
        int64_t now = now_ms();

        taskENTER_CRITICAL(&s_ble_lock);
        phase = s_phase;
        have_sync = s_have_sync;
        last_rx_ms = s_last_rx_ms;
        guard_deadline_ms = s_guard_scan_deadline_ms;
        taskEXIT_CRITICAL(&s_ble_lock);

        if (phase == BLE_PHASE_ACQUISITION) {
            start_continuous_scan();
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        if (phase == BLE_PHASE_LOCKED_WAIT) {
            int64_t wake_ms;

            stop_scanning_if_needed();
            if (!have_sync) {
                taskENTER_CRITICAL(&s_ble_lock);
                s_phase = BLE_PHASE_ACQUISITION;
                taskEXIT_CRITICAL(&s_ble_lock);
                continue;
            }

            wake_ms = last_rx_ms + (int64_t)SENSOR_INTERVAL_MS - (int64_t)GUARD_WINDOW_MS;
            if (now >= wake_ms) {
                taskENTER_CRITICAL(&s_ble_lock);
                s_phase = BLE_PHASE_LOCKED_SCAN;
                s_packet_seen_in_window = 0;
                s_guard_scan_deadline_ms = now + (int64_t)GUARD_WINDOW_MS;
                taskEXIT_CRITICAL(&s_ble_lock);

                start_continuous_scan();
                xSemaphoreGive(displayUpdateSemaphore);
                continue;
            }

            int64_t sleep_ms = wake_ms - now;
            if (sleep_ms > 5000) {
                sleep_ms = 5000;
            }
            vTaskDelay(pdMS_TO_TICKS((uint32_t)sleep_ms));
            continue;
        }

        if (phase == BLE_PHASE_LOCKED_SCAN) {
            start_continuous_scan();

            if (now >= guard_deadline_ms) {
                stop_scanning_if_needed();

                taskENTER_CRITICAL(&s_ble_lock);
                if (s_phase == BLE_PHASE_LOCKED_SCAN) {
                    if (!s_packet_seen_in_window) {
                        s_miss_count = (uint8_t)(s_miss_count + 1);
                        if (s_miss_count >= MAX_CONSECUTIVE_MISSES) {
                            s_phase = BLE_PHASE_ACQUISITION;
                            s_have_sync = 0;
                            s_miss_count = 0;
                        } else {
                            s_phase = BLE_PHASE_LOCKED_WAIT;
                        }
                    } else {
                        s_miss_count = 0;
                        s_phase = BLE_PHASE_LOCKED_WAIT;
                    }
                }
                taskEXIT_CRITICAL(&s_ble_lock);

                xSemaphoreGive(displayUpdateSemaphore);
            }

            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }
    }
}
#endif

static void set_pixel(uint16_t *plane, uint8_t row, uint8_t col) {
    if (row < 16 && col < 16) {
        plane[row] |= (1U << (15U - col));
    }
}

void Ble_Sensor_View__Initialize(void) {
#if CONFIG_BT_ENABLED
    if (s_ble_task_handle == NULL) {
        xTaskCreate(
            ble_scan_task,
            "BleScanTask",
            (4 * configMINIMAL_STACK_SIZE),
            NULL,
            5,
            &s_ble_task_handle
        );
    }
#else
    ESP_LOGW(TAG, "CONFIG_BT_ENABLED is off; BLE scanner view will show disabled state");
#endif
}

void Ble_Sensor_View__Get_frame(view_frame_t *frame) {
    if (!frame) {
        return;
    }

    memset(frame, 0, sizeof(*frame));

#if !CONFIG_BT_ENABLED
    for (uint8_t i = 0; i < 16; i++) {
        set_pixel(frame->red, i, i);
        set_pixel(frame->red, i, (uint8_t)(15 - i));
    }
    return;
#else
    ble_phase_t phase;
    uint8_t have_sync;
    uint8_t is_scanning;
    uint8_t miss_count;
    uint16_t sensor_id;
    int16_t temperature;

    taskENTER_CRITICAL(&s_ble_lock);
    phase = s_phase;
    have_sync = s_have_sync;
    is_scanning = s_is_scanning;
    miss_count = s_miss_count;
    sensor_id = s_sensor_id;
    temperature = s_temperature;
    taskEXIT_CRITICAL(&s_ble_lock);

    uint16_t border = 0xFFFF;
    if (phase == BLE_PHASE_ACQUISITION) {
        frame->blue[0] = border;
        frame->blue[15] = border;
    } else {
        frame->green[0] = border;
        frame->green[15] = border;
    }

    if (is_scanning) {
        for (uint8_t col = 6; col < 10; col++) {
            set_pixel(frame->green, 1, col);
            set_pixel(frame->blue, 1, col);
        }
    }

    if (have_sync) {
        set_pixel(frame->green, 3, 7);
        set_pixel(frame->green, 3, 8);
        set_pixel(frame->green, 4, 7);
        set_pixel(frame->green, 4, 8);
    } else {
        set_pixel(frame->red, 3, 7);
        set_pixel(frame->red, 3, 8);
        set_pixel(frame->red, 4, 7);
        set_pixel(frame->red, 4, 8);
    }

    for (uint8_t i = 0; i < miss_count && i < 3; i++) {
        set_pixel(frame->red, 6, (uint8_t)(2 + i));
        set_pixel(frame->red, 7, (uint8_t)(2 + i));
    }

    uint8_t nibble = (uint8_t)(sensor_id & 0x0F);
    for (uint8_t b = 0; b < 4; b++) {
        if (nibble & (1U << b)) {
            set_pixel(frame->blue, 2, (uint8_t)(12 + b));
            set_pixel(frame->blue, 3, (uint8_t)(12 + b));
        }
    }

    int16_t clamped = temperature;
    if (clamped > 80) {
        clamped = 80;
    }
    if (clamped < -40) {
        clamped = -40;
    }

    uint8_t row = (uint8_t)(((clamped + 40) * 15) / 120);
    row = (uint8_t)(15 - row);

    if (temperature >= 0) {
        set_pixel(frame->red, row, 10);
        set_pixel(frame->red, row, 11);
    } else {
        set_pixel(frame->blue, row, 10);
        set_pixel(frame->blue, row, 11);
    }
#endif
}

void Ble_Sensor_View__UI_Button(uint8_t btn) {
    (void)btn;
}

void Ble_Sensor_View__UI_Encoder_Top(uint8_t direction) {
    (void)direction;
}

void Ble_Sensor_View__UI_Encoder_Side(uint8_t direction) {
    if (direction == 0) {
        View__Change_brightness(0);
    } else {
        View__Change_brightness(1);
    }
}

uint32_t Ble_Sensor_View__Get_refresh_rate_ms(void) {
    return VIEW_REFRESH_MS;
}