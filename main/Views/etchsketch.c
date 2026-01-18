#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include "etchsketch.h"
#include "view.h"
#include "ui.h"
#include "mqtt.h"
#include "mqtt_protocol.h"

static const char *TAG = "WEATHER_STATION: ETCHSKETCH";

// Color enum for pixel drawing
typedef enum {
    RED = 0,
    GREEN = 1,
    BLUE = 2
} pixel_color_t;

// Private static variables
static uint8_t Position_row;
static uint8_t Position_col;

// Paint mode tracking
static uint8_t Paint_mode_active;      // 0=off, 1=paint with button, 2=paint with button2, 3=paint with button3
static pixel_color_t Paint_color;      // color for painting

static mqtt_shared_view_frame_t shared_view;

#define FLUSH_TIMER_PERIOD_MS 2000  // 2 seconds of inactivity triggers flush
static uint16_t Last_seq_seen;
static uint16_t Next_seq_to_send;

static uint8_t Shared_comm_active;      // 1=have recent comm and allowed to sync
static uint8_t Initial_full_sync; // 1=waiting for first frame from server

// Button state tracking for multi-button detection
static uint8_t Active_buttons = 0;      // Bitmask: bit 0 = btn1, bit 1 = btn2, bit 2 = btn3

// Timer for batching updates
static TimerHandle_t flush_timer = NULL;
// Worker to perform flush outside timer task
static TaskHandle_t flush_worker_task_handle = NULL;
static SemaphoreHandle_t flush_sem = NULL;

// Private method prototypes
static void flush_pending(void);
static void flush_timer_callback(TimerHandle_t timer);
static void flush_worker_task(void *pvParameters);
static void clear_shared_view(void);
static void queue_local_pixel(uint8_t row, uint8_t col, pixel_color_t color);
static void request_full_sync(void);

// PUBLIC METHODS

void Etchsketch__Initialize(void) {
    memset(&shared_view, 0, sizeof(shared_view));

    Last_seq_seen = 0;
    Next_seq_to_send = 0;
    Shared_comm_active = 0;
    Initial_full_sync = 0;

    if (flush_sem == NULL) {
        flush_sem = xSemaphoreCreateBinary();
        if (flush_sem == NULL) {
            ESP_LOGE(TAG, "Failed to create flush semaphore");
        }
    }

    Position_col = 0;
    Position_row = 0;
    Paint_mode_active = 0;
    Paint_color = 0;

    // Create the flush timer (don't start it yet)
    if (flush_timer == NULL) {
        flush_timer = xTimerCreate(
            "FlushTimer",                  // Timer name
            pdMS_TO_TICKS(FLUSH_TIMER_PERIOD_MS),  // Period (2 seconds)
            pdFALSE,                       // Auto-reload (false = one-shot)
            NULL,                          // Timer ID
            flush_timer_callback           // Callback function
        );
        if (flush_timer == NULL) {
            ESP_LOGE(TAG, "Failed to create flush timer");
        }
    }

    // Create worker task to perform flush outside timer context
    if (flush_worker_task_handle == NULL) {
        BaseType_t created = xTaskCreate(
            flush_worker_task,
            "FlushWorker",
            3072,          // stack words; generous to avoid overflow
            NULL,
            6,             // medium priority
            &flush_worker_task_handle
        );
        if (created != pdPASS) {
            ESP_LOGE(TAG, "Failed to create flush worker task");
        }
    }
}

void Etchsketch__On_Enter(void) {
    // Use live broker connection status; ignore stale server-activity flag
    if (Mqtt__Is_connected()) {
        clear_shared_view();
        Shared_comm_active = 1;
        Initial_full_sync = 1;    // expecting full replacement from server
        request_full_sync();
    } else {
        Shared_comm_active = 0;
        ESP_LOGW(TAG, "No broker comm on enter; staying offline for shared view");
    }
}

void Etchsketch__Get_view(view_frame_t *frame) {
    memcpy(frame->red, shared_view.red, sizeof(shared_view.red));
    memcpy(frame->green, shared_view.green, sizeof(shared_view.green));
    memcpy(frame->blue, shared_view.blue, sizeof(shared_view.blue));
    frame->red[Position_row] |= (1 << Position_col);
}

// Methods performed on UI events (encoder/button presses)
void Etchsketch__UI_Encoder_Top(uint8_t direction) {
    // Move cursor right/left
    if(direction == 0) {
        Position_col++;
        if(Position_col > 15) {
            Position_col = 0;
        }
    } else {
        if(Position_col > 0) {
            Position_col --;
        } else {
            Position_col = 15;
        }
    }
    
    // If paint mode active, paint at new position
    if (Paint_mode_active) {
        queue_local_pixel(Position_row, Position_col, Paint_color);
    }
}

void Etchsketch__UI_Encoder_Side(uint8_t direction) {
    // Move cursor up/down
    if(direction == 0) {
        Position_row++;
        if(Position_row > 15) {
            Position_row = 0;
        }
    } else {
        if(Position_row > 0) {
            Position_row--;
        } else {
            Position_row = 15;
        }
    }
    
    // If paint mode active, paint at new position
    if (Paint_mode_active) {
        queue_local_pixel(Position_row, Position_col, Paint_color);
    }
}

void Etchsketch__UI_Button(uint8_t btn) {
    // Track button state
    if (btn >= 1 && btn <= 3) {
        Active_buttons |= (1 << (btn - 1));
    }
    
    // Check if all three buttons are pressed simultaneously
    if ((Active_buttons & 0x7) == 0x7) {  // All 3 bits set (0b111)
        clear_shared_view();
        Paint_mode_active = 0;
        return;
    }
    
    // Button press: enter paint mode and paint current position
    if (btn == 1) {
        Paint_mode_active = 1;
        Paint_color = RED;
        queue_local_pixel(Position_row, Position_col, Paint_color);
    } else if (btn == 2) {
        Paint_mode_active = 1;
        Paint_color = GREEN;
        queue_local_pixel(Position_row, Position_col, Paint_color);
    } else if (btn == 3) {
        Paint_mode_active = 1;
        Paint_color = BLUE;
        queue_local_pixel(Position_row, Position_col, Paint_color);
    }
}

// Button released: exit paint mode
void Etchsketch__UI_Button_Released(uint8_t btn) {
    // Clear the button bit
    if (btn >= 1 && btn <= 3) {
        Active_buttons &= ~(1 << (btn - 1));
    }
    
    // Exit paint mode on any button release
    if (Paint_mode_active) {
        Paint_mode_active = 0;
    }
}

void Etchsketch__Apply_remote_frame(const mqtt_shared_view_frame_t *frame) {
    if (!frame || !Shared_comm_active) return;
    Last_seq_seen = frame->seq;
    
    if (Initial_full_sync) {
        // First frame after entering view: replace entirely
        memcpy(&shared_view, frame, sizeof(shared_view));
        Initial_full_sync = 0;
    } else {
        // Subsequent frames: merge with OR to combine all device changes
        for (uint8_t row = 0; row < 16; row++) {
            shared_view.red[row] |= frame->red[row];
            shared_view.green[row] |= frame->green[row];
            shared_view.blue[row] |= frame->blue[row];
        }
    }
}

// PRIVATE METHODS

// Publish mqtt message, get full shared view from server
static void request_full_sync(void) {
    if (!Shared_comm_active) return;
    uint8_t buffer[MQTT_PROTOCOL_HEADER_SIZE];
    int len = mqtt_protocol_build_shared_view_request(buffer, sizeof(buffer));
    if (len > 0) {
        Mqtt__Publish(MQTT_TOPIC_SHARED_VIEW, buffer, len);
    }
}

static void clear_shared_view(void) {
    memset(&shared_view, 0, sizeof(shared_view));
}

static void queue_local_pixel(uint8_t row, uint8_t col, pixel_color_t color) {
    // Ensure row and col fit in 4 bits (0-15 range)
    if (row >= 16 || col >= 16) {
        ESP_LOGW(TAG, "Invalid pixel coordinates: row=%d, col=%d", row, col);
        return;
    }
    
    // Toggle bit locally
    uint16_t bit = (1 << col);
    uint16_t *row_ptr = NULL;
    if (color == RED) row_ptr = &shared_view.red[row];
    else if (color == GREEN) row_ptr = &shared_view.green[row];
    else if (color == BLUE) row_ptr = &shared_view.blue[row];
    
    // Update local pixel state
    if (row_ptr) {
        *row_ptr ^= bit;  // toggle the bit
    }
    
    // Start flush timer on any change
    if (Shared_comm_active) {
        xTimerReset(flush_timer, 0);
    }
}

// Send full shared view to server
static void flush_pending(void) {
    if(!Mqtt__Is_connected()) {
        Shared_comm_active = 0;
        return;
    }
    // Prepare frame with sequence number
    mqtt_shared_view_frame_t frame_to_send = shared_view;
    frame_to_send.seq = Next_seq_to_send;
    
    // Publish full view
    Mqtt__Publish(MQTT_TOPIC_SHARED_VIEW, (uint8_t *)&frame_to_send, sizeof(frame_to_send));
    Next_seq_to_send++;
    
    // Stop the flush batching timer since we've sent
    if (flush_timer != NULL) {
        xTimerStop(flush_timer, 0);
    }
}

// Timer callback: called after 2 seconds of inactivity
static void flush_timer_callback(TimerHandle_t timer) {
    // Keep timer task lean: just notify worker
    if (flush_sem != NULL) {
        xSemaphoreGive(flush_sem);
    }
}

// Worker task: waits for timer signal then flushes pending updates
static void flush_worker_task(void *pvParameters) {
    (void)pvParameters;
    for (;;) {
        if (flush_sem != NULL) {
            xSemaphoreTake(flush_sem, portMAX_DELAY);
            flush_pending();
        }
    }
}

