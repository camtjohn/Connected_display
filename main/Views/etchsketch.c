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

// Private static variables
static uint8_t Position_row;
static uint8_t Position_col;

// Paint mode tracking
static uint8_t Paint_mode_active;      // 0=off, 1=paint with button, 2=paint with button2, 3=paint with button3
static uint8_t Paint_color;            // 0=red, 1=green, 2=blue

// Inline shared view state and batching
static mqtt_shared_view_frame_t shared_view;

#define MAX_PENDING_UPDATES 32
#define FLUSH_THRESHOLD 32  // Safety limit to prevent buffer overflow (timer is primary trigger)
#define FLUSH_TIMER_PERIOD_MS 2000  // 2 seconds of inactivity triggers flush
static mqtt_shared_pixel_update_t Pending_updates[MAX_PENDING_UPDATES];
static uint8_t Pending_count;
static uint8_t Batch_active;
static uint16_t Last_seq_seen;
static uint16_t Next_seq_to_send;

// Timer for batching updates
static TimerHandle_t flush_timer = NULL;
// Worker to perform flush outside timer task
static TaskHandle_t flush_worker_task_handle = NULL;
static SemaphoreHandle_t flush_sem = NULL;

static void apply_pixel_local(uint8_t row, uint8_t col, uint8_t color);
static void flush_pending(void);
static void flush_timer_callback(TimerHandle_t timer);
static void reset_flush_timer(void);
static void flush_worker_task(void *pvParameters);

// Private method prototypes

// PUBLIC METHODS

void Etchsketch__Initialize(void) {
    memset(&shared_view, 0, sizeof(shared_view));

    Pending_count = 0;
    Batch_active = 0;
    Last_seq_seen = 0;
    Next_seq_to_send = 0;

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
    Etchsketch__Request_full_sync();
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
        Etchsketch__Queue_local_pixel(Position_row, Position_col, Paint_color);
        ESP_LOGI(TAG, "Paint trail at row=%d col=%d (color=%d)", Position_row, Position_col, Paint_color);
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
        Etchsketch__Queue_local_pixel(Position_row, Position_col, Paint_color);
        ESP_LOGI(TAG, "Paint trail at row=%d col=%d (color=%d)", Position_row, Position_col, Paint_color);
    }
}

void Etchsketch__UI_Button(uint8_t btn) {
    // Button press: enter paint mode and paint current position
    if (btn == 1) {
        Etchsketch__Begin_batch();
        Paint_mode_active = 1;
        Paint_color = 0;  // Red
        Etchsketch__Queue_local_pixel(Position_row, Position_col, Paint_color);
        ESP_LOGI(TAG, "Button 1 pressed - Paint mode active (RED) at row=%d col=%d", Position_row, Position_col);
    } else if (btn == 2) {
        Etchsketch__Begin_batch();
        Paint_mode_active = 1;
        Paint_color = 1;  // Green
        Etchsketch__Queue_local_pixel(Position_row, Position_col, Paint_color);
        ESP_LOGI(TAG, "Button 2 pressed - Paint mode active (GREEN) at row=%d col=%d", Position_row, Position_col);
    } else if (btn == 3) {
        Etchsketch__Begin_batch();
        Paint_mode_active = 1;
        Paint_color = 2;  // Blue
        Etchsketch__Queue_local_pixel(Position_row, Position_col, Paint_color);
        ESP_LOGI(TAG, "Button 3 pressed - Paint mode active (BLUE) at row=%d col=%d", Position_row, Position_col);
    }
}

// Button released: exit paint mode
void Etchsketch__UI_Button_Released(uint8_t btn) {
    // Exit paint mode on any button release
    if (Paint_mode_active) {
        Etchsketch__End_batch();
        ESP_LOGI(TAG, "Button %d released - Paint mode inactive", btn);
        Paint_mode_active = 0;
    }
}

// Inline shared view helpers
void Etchsketch__Request_full_sync(void) {
    uint8_t buffer[MQTT_PROTOCOL_HEADER_SIZE];
    int len = mqtt_protocol_build_shared_view_request(buffer, sizeof(buffer));
    if (len > 0) {
        Mqtt__Publish(MQTT_TOPIC_SHARED_VIEW, buffer, len);
    }
}

void Etchsketch__Apply_remote_frame(const mqtt_shared_view_frame_t *frame) {
    if (!frame) return;
    Last_seq_seen = frame->seq;
    memcpy(&shared_view, frame, sizeof(shared_view));
}

void Etchsketch__Apply_remote_updates(const mqtt_shared_pixel_update_t *updates, uint8_t count, uint16_t seq) {
    if (!updates || count == 0) return;
    
    // Detect seq gap
    if (seq != Last_seq_seen + 1) {
        ESP_LOGW(TAG, "Shared view seq gap detected: expected %u, got %u", Last_seq_seen + 1, seq);
        Etchsketch__Request_full_sync();
    }
    Last_seq_seen = seq;
    
    for (uint8_t i = 0; i < count; i++) {
        apply_pixel_local(updates[i].row, updates[i].col, updates[i].color);
    }
}

void Etchsketch__Begin_batch(void) { Batch_active = 1; }
void Etchsketch__End_batch(void) { 
    Batch_active = 0; 
    // Don't flush immediately - let the timer handle it
    // This allows the user to press another button within 2 seconds without sending data
}

void Etchsketch__Queue_local_pixel(uint8_t row, uint8_t col, uint8_t color) {
    // Ensure row and col fit in 4 bits (0-15 range)
    if (row >= 16 || col >= 16) {
        ESP_LOGW(TAG, "Invalid pixel coordinates: row=%d, col=%d", row, col);
        return;
    }
    
    apply_pixel_local(row, col, color);

    // Check if pixel already pending; update color if so
    uint8_t found_duplicate = 0;
    for (uint8_t i = 0; i < Pending_count; i++) {
        if (Pending_updates[i].row == row && Pending_updates[i].col == col) {
            Pending_updates[i].color = color;
            found_duplicate = 1;
            break;
        }
    }

    // Add new pending entry if not a duplicate
    if (!found_duplicate && Pending_count < MAX_PENDING_UPDATES) {
        Pending_updates[Pending_count].row = row;
        Pending_updates[Pending_count].col = col;
        Pending_updates[Pending_count].color = color;
        Pending_count++;
        
        // Flush if buffer is full to make room for more pixels
        if (Pending_count >= MAX_PENDING_UPDATES) {
            flush_pending();
            // Reset timer after flush so inactivity countdown restarts
            reset_flush_timer();
        }
    }

    // Reset the flush timer on each new update
    // This ensures updates only send after 2 seconds of complete inactivity
    reset_flush_timer();
}

// Update display frame locally
static void apply_pixel_local(uint8_t row, uint8_t col, uint8_t color) {
    if (row >= 16 || col >= 16) return;
    uint16_t bit = (1 << col);
    switch (color) {
        case 0: shared_view.red[row] |= bit; break;
        case 1: shared_view.green[row] |= bit; break;
        case 2: shared_view.blue[row] |= bit; break;
        default: break;
    }
}

// Send batch of pixel updates to shared view
static void flush_pending(void) {
    if (Pending_count == 0) return;
    uint8_t buffer[MQTT_PROTOCOL_HEADER_SIZE + 3 + (MAX_PENDING_UPDATES * 3)];
    int len = mqtt_protocol_build_shared_view_updates(Pending_updates, Pending_count, Next_seq_to_send, buffer, sizeof(buffer));
    if (len > 0) {
        Mqtt__Publish(MQTT_TOPIC_SHARED_VIEW, buffer, len);
        Next_seq_to_send++;
    }
    Pending_count = 0;
    
    // Stop the timer since we've flushed
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

// Reset the flush timer (restarts it from zero)
static void reset_flush_timer(void) {
    if (flush_timer != NULL) {
        // Reset the timer - this stops and restarts it from 0
        xTimerReset(flush_timer, 0);
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

