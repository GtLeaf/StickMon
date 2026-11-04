/*
 * Copyright (c) 2026 深圳希德创新网络有限公司 (SDGOODS)
 * SPDX-License-Identifier: Apache-2.0
 * Adapted from the SDGOODS BSP panel assembly. Keep SDGOODS NOTICE with distributions.
 * The vendor command table and ST77916 driver are referenced unchanged from the SDK.
 */
#include "NativePanel.h"
#include "board_pins.h"
#include "esp_lcd_st77916.h"
#include "st77916_vendor_init.inc"

esp_err_t stickmon_sdgoods_panel_create(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t* panel) {
    st77916_vendor_config_t vendor = {
        .init_cmds = vendor_specific_init_vendor,
        .init_cmds_size = sizeof(vendor_specific_init_vendor) / sizeof(vendor_specific_init_vendor[0]),
        .flags = {.use_qspi_interface = 1, .init_madctl_valid = 1},
        .init_madctl = BOARD_ST77916_VENDOR_MADCTL,
    };
    esp_lcd_panel_dev_config_t config = {
        .reset_gpio_num = BOARD_LCD_GPIO_RST,
        .rgb_ele_order = BOARD_LCD_RGB_ORDER_BGR ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor,
    };
    esp_err_t err = esp_lcd_new_panel_st77916(io, &config, panel);
    if (err == ESP_OK) err = esp_lcd_panel_reset(*panel);
    if (err == ESP_OK) err = esp_lcd_panel_init(*panel);
    if (err == ESP_OK) err = esp_lcd_panel_st77916_set_madctl(*panel, BOARD_ST77916_VENDOR_MADCTL);
    if (err == ESP_OK) err = esp_lcd_panel_disp_on_off(*panel, true);
    return err;
}
