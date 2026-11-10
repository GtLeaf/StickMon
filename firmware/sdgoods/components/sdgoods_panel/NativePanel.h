// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t stickmon_sdgoods_panel_create(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t* panel);
// SLPIN/SLPOUT; the SDK ST77916 driver only exposes display on/off.
// After waking, wait 120ms before display on; GRAM content is retained.
esp_err_t stickmon_sdgoods_panel_sleep(esp_lcd_panel_io_handle_t io, bool sleep);
#ifdef __cplusplus
}
#endif
