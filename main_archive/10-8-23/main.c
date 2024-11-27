#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include <math.h>
#include "main.h"
#include "wifi.h"
#include "array_tools.h"
#include "adc_tools.h"
#include "mqtt.h"

int decide_msg_type(float, float, float); 
void publish_msg(int, float); 
float get_temp(void);
void change_cooling_pump(int);
void change_heater(int);


void app_main(void)
{
    // Configured relay pin
    gpio_reset_pin(PIN_RELAY_PUMP);
    gpio_reset_pin(PIN_RELAY_HEATER);
    gpio_set_direction(PIN_RELAY_PUMP, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_RELAY_HEATER, GPIO_MODE_OUTPUT);
    
    start_wifi();
    start_mqtt();
    adc_init();

    FERMENTER_STATE f_state = STANDBY;

    // If temp reading is an amount different than prev, read again once
    int read_again = 0;

    // Use following to determine whether to publish new message
    float temp_fahr;
    float prev_reading;
    float reading_before_prev = 0.0;     // Most recent past reading different from previous
    int pump_run_iteration = 0;

    int pub_msg_type;           // 0=no pub, 1=normal, 2=send notification


    // Read/publish for first time
    temp_fahr = get_temp();
    prev_reading = temp_fahr;
    publish_msg(1, temp_fahr);

    while (1) {
        // If prev temp reading = temp reading before that, no need to change records
        // Otherwise, record prev reading and reading before that
        if (temp_fahr != prev_reading) {
            reading_before_prev = prev_reading;
            prev_reading = temp_fahr;
        }

        temp_fahr = get_temp();
        printf("TEMP: %f\n", temp_fahr);

        // Decide whether to publish and if so, what topic to use
        pub_msg_type = decide_msg_type(temp_fahr, prev_reading, reading_before_prev);

        if (pub_msg_type == 9) {    // Temp read significantly different from prev
            if (read_again == 0) {
                read_again = 1;
                // Reset history
                temp_fahr = prev_reading;
                printf("Reading temp again\n");
                vTaskDelay(pdMS_TO_TICKS(2000));    // Pause for 2 sec
                continue;   // Return to beginning of while loop, read again.
            } else {        // Already re-read, don't want to read again. 
                read_again = 0;
                pub_msg_type = 1;
            }
        }

        publish_msg(pub_msg_type, temp_fahr);

        printf("STATE: %d\n", f_state);
        switch (f_state) {
            case STANDBY:
                // Reset conditions that may have been modified
                pump_run_iteration = 0;
                read_again = 0;

                // Check if temp outside of thresholds. Enabled device if needed.
                if (temp_fahr < LOWER_THRESH) {
                    change_heater(1);
                    f_state = HEATER_ENABLED;
                    pub_mqtt(MQTT_TOPIC_FROM_THERM, "Heater enabled");
                    vTaskDelay(pdMS_TO_TICKS(POLL_TIME_HEATER_ON));
                    break;
                } else if (temp_fahr > UPPER_THRESH) {
                    change_cooling_pump(1);
                    f_state = COOLING_PUMP_ENABLED;
                    pub_mqtt(MQTT_TOPIC_FROM_THERM, "Cooling Pump Enabled");
                    vTaskDelay(pdMS_TO_TICKS(POLL_TIME_PUMP_ON));
                    break;
                }

                // Temp in range. Forge on normal.
                vTaskDelay(pdMS_TO_TICKS(POLL_TIME_STANDBY));
                break;

            case COOLING_PUMP_ENABLED:
                // Is temp back within threshold? OR Has the pump been running for NUM_ITERATIONS?
                if ((temp_fahr < UPPER_THRESH) || (pump_run_iteration > NUM_ITERATIONS_RUN_PUMP)) {
                    change_cooling_pump(0);
                    f_state = STANDBY;
                    pump_run_iteration = 0;
                    pub_mqtt(MQTT_TOPIC_FROM_THERM, "Cooling Pump Disabled");
                    vTaskDelay(pdMS_TO_TICKS(POLL_TIME_STANDBY));
                    break;
                }

                pump_run_iteration ++;
                vTaskDelay(pdMS_TO_TICKS(POLL_TIME_PUMP_ON));
                break;

            case HEATER_ENABLED:
                // Temp back within threshold?
                if (temp_fahr > LOWER_THRESH) {
                    change_heater(0);
                    f_state = STANDBY;
                    pub_mqtt(MQTT_TOPIC_FROM_THERM, "Heater Disabled");
                    vTaskDelay(pdMS_TO_TICKS(POLL_TIME_STANDBY));
                    break;
                }
                // Heater still enabled
                vTaskDelay(pdMS_TO_TICKS(POLL_TIME_HEATER_ON));
                break;

            default:
                f_state = STANDBY;
                printf("Error, no state detected. Setting to STANDBY");
        }
    }

    adc_teardown();
}


// Publish new temp If new reading more than 0.25 different from previous
// OR
// ( If new reading is different from previous reading
//      AND
//  New is different from the past recording value that different from previous )
//   note: Prevents publishing  when temp on edge: 70.25, 70, 70.25, 70
int decide_msg_type(float temp_fahr, float prev_reading, float reading_before) {
    // If reading is more than 0.25 different from previous, re-read once. Otherwise publish normal
    if ((temp_fahr > (prev_reading + 0.25)) || (temp_fahr < (prev_reading - 0.25))) {
        return 9;
    }

    // If reading different than previous and from one before that, publish msg
    if ((temp_fahr != prev_reading) && (temp_fahr != reading_before)) {
        return 1;

    } else { return 0; }   // Otherwise do not publish
}

void publish_msg(int msg_type, float temp) {
        // Convert float to string
        char msg[7];
        sprintf(msg, "%g", temp);

        if (msg_type == 1) {
            pub_mqtt(MQTT_TOPIC_FROM_THERM, msg);

        } else if (msg_type == 2) {
            pub_mqtt(MQTT_TOPIC_FROM_THERM, msg);
            vTaskDelay(pdMS_TO_TICKS(1000));    // Not sure if needed, but delay 1 sec
            pub_mqtt(MQTT_TOPIC_FROM_THERM_NOTIFY, msg);
        }
}

// Read thermistor. ADC output requires multiple readings, averaging.
float get_temp(void) {
    // GET MULTIPLE READINGS FROM ADC. FIND MEDIAN. TRANSLATE TO TEMP FAHR.
    uint32_t i_read_adc = 0;
    int adc_readings[ADC_NUM_READINGS];

    // Read adc. Put mv readings into array
    while(i_read_adc < ADC_NUM_READINGS) {
        adc_readings[i_read_adc] = read_adc() - ADJUST_MV_READING;

        vTaskDelay(pdMS_TO_TICKS(20));
        i_read_adc++;
    }

    int median_reading = find_median(adc_readings, ADC_NUM_READINGS);

    // Voltage divider
    float Vs = 3.3;
    float Vin = median_reading * 0.001;
    float Rs = 10000.0;     // Resistor in series with thermistor
    int r_thermistor = (int)((Rs*Vin)/(Vs-Vin));

    // USE BETA VALUE TO FIND TEMP FROM RESISTANCE VALUE
    // Translate resistance to deg Cels
    //float temp_cels_thermistor = (1.0/ ( (log(r_thermistor/Rs)/3892.0) + (1.0/298.0) ) ) - 273.0;
    // Cels to Fahr
    //temp_fahr = ((temp_cels_thermistor * 9.0) / 5.0) + 32.0;
    //ESP_LOGI(TAG, "ResTherm: %f   VoltR: %d  Temp F: %f",r_thermistor, median_reading, temp_fahr);

    // USE LOOKUP TABLE TO FIND TEMP FROM RESISTANCE VALUE
    int resist[] = {19820, 19455, 19215, 18978, 18744, 18512, 18284, 18058, 17835, 17615, 17398, 17183, 16971, 16762, 16555, 16351, 16149, 15950, 15680, 15558, 15366, 15271, 15177, 15083, 14989, 14897, 14805, 14713, 14622, 14531, 14441, 14352, 14263, 14175, 14087, 14000, 13913, 13827, 13742, 13657, 13572, 13488, 13405, 13322, 13239, 13157, 13076, 12995, 12914, 12835, 12755, 12676, 12490, 12520, 12442, 12365, 12289, 12213, 12137, 12062, 11987, 11913, 11839, 11766, 11693, 11621, 11549, 11478, 11406, 11336, 11266, 11196, 11127, 11058, 10989, 10921, 10854, 10787, 10720, 10654, 10588, 10522, 10457, 10392, 10328, 10264, 10200, 10137, 10000, 10012, 9950, 9889, 9827, 9767, 9706, 9646, 9586, 9527, 9468, 9410, 9351, 9293, 9236, 9179, 9122, 9065, 9009, 8954, 8898, 8843, 8788, 8734, 8680, 8626, 8573, 8520, 8467, 8415, 8363, 8311, 8259, 8157, 8053, 7957, 7859, 7762, 7666, 7572, 7478, 7386, 7295, 7205, 7116, 7028, 6941, 6856, 6771, 6688, 6605, 6524, 6520};
    float temp[] = {50, 50.5, 51, 51.5, 52, 52.5, 53, 53.5, 54, 54.5, 55, 55.5, 56, 56.5, 57, 57.5, 58, 58.5, 59, 59.5, 60, 60.25, 60.5, 60.75, 61, 61.25, 61.5, 61.75, 62, 62.25, 62.5, 62.75, 63, 63.25, 63.5, 63.75, 64, 64.25, 64.5, 64.75, 65, 65.25, 65.5, 65.75, 66, 66.25, 66.5, 66.75, 67, 67.25, 67.5, 67.75, 68, 68.25, 68.5, 68.75, 69, 69.25, 69.5, 69.75, 70, 70.25, 70.5, 70.75, 71, 71.25, 71.5, 71.75, 72, 72.25, 72.5, 72.75, 73, 73.25, 73.5, 73.75, 74, 74.25, 74.5, 74.75, 75, 75.25, 75.5, 75.75, 76, 76.25, 76.5, 76.75, 77, 77.25, 77.5, 77.75, 78, 78.25, 78.5, 78.75, 79, 79.25, 79.5, 79.75, 80, 80.25, 80.5, 80.75, 81, 81.25, 81.5, 81.75, 82, 82.25, 82.5, 82.75, 83, 83.25, 83.5, 83.75, 84, 84.25, 84.5, 84.75, 85, 85.5, 86, 86.5, 87, 87.5, 88, 88.5, 89, 89.5, 90, 90.5, 91, 91.5, 92, 92.5, 93, 93.5, 94, 94.5, 95};
    int num_elem = 141;
    float temp_fahr;

    // Lookup resistance in table, find index of relating temperature
    for(int i=0; i<num_elem; i++) {
        if(r_thermistor > resist[i]) {
            // Temp is lower than the lowest calibration
            if(i == 0) {
                temp_fahr = -1;
                break;
            }

            // Found the correct interval. Which index is the value closest to?
            int upper_interval_diff = resist[i-1] - r_thermistor;
            int lower_interval_diff = r_thermistor - resist[i];
            if(upper_interval_diff > lower_interval_diff) {
                temp_fahr = temp[i];    // lower index closer
            } else {temp_fahr = temp[i-1];}
            break;
        }
        // Traversed array. Temp is higher than calibration allows
        if(i == (num_elem-1)) {
            temp_fahr = 999;
        }
    }
    ESP_LOGI(TAG, "ResTherm: %d   VoltR: %d  Temp F: %f",r_thermistor, median_reading, temp_fahr);

    return (temp_fahr);
}

void change_cooling_pump(int pump_state) {
    gpio_set_level(PIN_RELAY_PUMP, pump_state);
    printf("Pump changed state\n");
}

void change_heater(int heater_state) {
    gpio_set_level(PIN_RELAY_HEATER, heater_state);
    printf("Heater changed state\n");
}