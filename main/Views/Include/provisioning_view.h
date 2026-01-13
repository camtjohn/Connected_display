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

#endif // PROVISIONING_VIEW_H
