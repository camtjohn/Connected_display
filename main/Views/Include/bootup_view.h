#ifndef BOOTUP_VIEW_H
#define BOOTUP_VIEW_H

#include "view.h"

/**
 * @brief Initialize bootup view with random pixel pattern
 */
void Bootup_View__Initialize(void);

/**
 * @brief Get the bootup view frame with random colorful pixels
 * 
 * @param frame Pointer to view_frame_t to fill with LED data
 */
void Bootup_View__Get_frame(view_frame_t *frame);

/**
 * @brief Handle button press in bootup view (no-op)
 */
void Bootup_View__UI_Button(uint8_t btn);

/**
 * @brief Handle top encoder events (no-op)
 */
void Bootup_View__UI_Encoder_Top(uint8_t direction);

/**
 * @brief Handle side encoder events (no-op)
 */
void Bootup_View__UI_Encoder_Side(uint8_t direction);

#endif // BOOTUP_VIEW_H
