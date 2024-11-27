#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "main.h"
#include "led_driver.h"
#include "spi.h"
#include "view.h"
#include "wifi.h"
#include "mqtt.h"

#define ENABLE_DISPLAY 1

// VIEW
uint16_t View_red[16];
uint16_t View_green[16];
uint16_t View_blue[16];
static uint8_t Brightness;

// TASK
TaskHandle_t periodicTaskHandle = NULL;
void periodic_task_display_view(void *);
// Example suspend/resume task:
// vTaskSuspend(periodicTaskHandle);
// vTaskResume(periodicTaskHandle);

// PRIVATE METHODS

void setup_gpio_input(uint8_t);


void app_main(void) {
    Wifi__Start();
    Mqtt__Start();
    //Done on connection in mqtt even handler: Mqtt__Subscribe(MQTT_TOPIC_DATA_UPDATE);

    #if(ENABLE_DISPLAY) 
    View__Initialize();
    Led_driver__Initialize();
    Led_driver__Setup();
    Brightness = PWM_DUTY & 0xF;

    // TEST BUTTONS
    setup_gpio_input(BTN_1);
    setup_gpio_input(BTN_2);
    setup_gpio_input(BTN_3);
    setup_gpio_input(BTN_4);


    // TASK
    // Create the task with a priority of your choice
    xTaskCreate(
        periodic_task_display_view,          // Task function
        "PeriodicTask",        // Task name (for debugging)
        configMINIMAL_STACK_SIZE, // Stack size (words)
        NULL,                  // Task parameter
        5,  // Task priority
        &periodicTaskHandle    // Task handle
    );
    #endif

    while (1) {
        View__Get_views(View_red, View_green, View_blue);
        Led_driver__Update_RAM(View_red, View_green, View_blue);
        vTaskDelay(pdMS_TO_TICKS(UPDATE_VIEW_EVERY_MS));  //every 30sec
    }
}


// PRIVATE METHODS

void setup_gpio_input(uint8_t pin) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
}


// Define task: Continuously display view unless task paused/suspended
void periodic_task_display_view(void *pvParameters) {
    // Initialize the xLastWakeTime variable with the current tick count before method runs
    TickType_t time_start_task = xTaskGetTickCount();
    // After method runs, delay for this amount of ms
    const TickType_t run_every_ms = pdMS_TO_TICKS(TASK_PERIOD_MS);
    uint8_t btn3_pressed = 0;
    uint8_t btn4_pressed = 0;
    while (1) {
        // If press btn, latch and do something. Only unlatch if detect no press.
        if(btn3_pressed) {
            if(gpio_get_level(BTN_3)==0) {
                btn3_pressed=0;
            }
        } else {
            if(gpio_get_level(BTN_3)) {
                if(Brightness > 0) { 
                    Brightness--;
                }
                Led_driver__Set_brightness(Brightness);
                btn3_pressed=1;
            }
        }

        if(btn4_pressed) {
            if(gpio_get_level(BTN_4)==0) {
                btn4_pressed=0;
            }
        } else {
            if(gpio_get_level(BTN_4)) {
                if(Brightness < 15) {
                    Brightness++;
                }

                Led_driver__Set_brightness(Brightness);
                btn4_pressed=1;
            }
        }

        //printf("inside task looping...\n");
        vTaskDelayUntil(&time_start_task, run_every_ms);
    }
}