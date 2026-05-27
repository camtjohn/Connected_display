#ifndef BLE_SENSOR_VIEW_H
#define BLE_SENSOR_VIEW_H

#include "view.h"

void Ble_Sensor_View__Initialize(void);
void Ble_Sensor_View__Get_frame(view_frame_t *frame);
void Ble_Sensor_View__UI_Button(uint8_t btn);
void Ble_Sensor_View__UI_Encoder_Top(uint8_t direction);
void Ble_Sensor_View__UI_Encoder_Side(uint8_t direction);
uint32_t Ble_Sensor_View__Get_refresh_rate_ms(void);

#endif