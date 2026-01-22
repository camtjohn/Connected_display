#ifndef PROVISIONING_VIEW_H
#define PROVISIONING_VIEW_H

#include "view.h"

/**
 * @brief Initialize provisioning view
 */
void Provisioning_View__Initialize(void);

/**
 * @brief Get the provisioning view frame
 * Displays "PROVISIONING" or similar indicator on LED display
 * 
 * @param frame Pointer to view_frame_t to fill with LED data
 */
void Provisioning_View__Get_frame(view_frame_t *frame);

/**
 * @brief Handle button press in provisioning view
 * Button 1: Start provisioning (enter WiFi credentials)
 * Button 4: Continue offline without provisioning
 * 
 * @param btn Button number (1-3, corresponding to buttons 2-4)
 */
void Provisioning_View__UI_Button(uint8_t btn);

/**
 * @brief Handle top encoder events (not used in provisioning view)
 */
void Provisioning_View__UI_Encoder_Top(uint8_t direction);

/**
 * @brief Handle side encoder events (not used in provisioning view)
 */
void Provisioning_View__UI_Encoder_Side(uint8_t direction);

/**
 * @brief Set provisioning context (no credentials vs failed connection)
 * @param has_credentials 0 = no credentials, 1 = has credentials but connection failed
 */
void Provisioning_View__Set_context(uint8_t has_credentials);

/**
 * @brief Check if user has taken an action (pressed button) in provisioning view
 * @return 1 if user pressed button, 0 otherwise
 */
int Provisioning_View__Get_user_action(void);

/**
 * @brief Check if currently in SoftAP mode (waiting for provisioning)
 * @return 1 if in SoftAP mode, 0 otherwise
 */
int Provisioning_View__Is_in_softap_mode(void);

#endif // PROVISIONING_VIEW_H
