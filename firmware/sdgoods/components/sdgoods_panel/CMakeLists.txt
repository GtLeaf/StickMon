# SPDX-License-Identifier: Apache-2.0
set(BOARD_ROOT "${STICKMON_SDGOODS_SDK_ROOT}/components/sdgoods_board")
idf_component_register(SRCS "NativePanel.c" "${BOARD_ROOT}/lcd/esp_lcd_st77916/esp_lcd_st77916.c"
    INCLUDE_DIRS "." "${BOARD_ROOT}/include" "${BOARD_ROOT}/lcd/esp_lcd_st77916"
    PRIV_INCLUDE_DIRS "${BOARD_ROOT}/lcd"
    REQUIRES esp_lcd esp_driver_gpio esp_driver_spi)
