set(STICKMON_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

include("${STICKMON_ROOT}/firmware/common/amoled_sources.cmake")

set(STICKMON_MAIN_REQUIRES
    esp_lcd
    esp_driver_i2c
    esp_codec_dev
    esp_event
    esp_netif
    esp_psram
    esp_system
    esp_timer
    esp_pm
    esp_driver_gpio
    esp_wifi
    nvs_flash
    spiffs
    espressif__esp_lcd_touch
    waveshare__esp32_s3_touch_amoled_1_8
)
if(STICKMON_ENABLE_CLAW)
    list(APPEND STICKMON_MAIN_REQUIRES
        stickmon_claw
        claw_core
    )
endif()

idf_component_register(
    SRCS
        "main.cpp"
        "${STICKMON_ROOT}/firmware/common/Rgb565Transfer.cpp"
        ${STICKMON_AMOLED_APP_SOURCES}
        "AmoledPlatform.cpp"
        "${STICKMON_ROOT}/firmware/common/Qmi8658.cpp"
        "${STICKMON_ROOT}/firmware/common/Axp2101Battery.cpp"
        ${STICKMON_AMOLED_UI_SOURCES}
        "TouchInput.cpp"
        ${STICKMON_AMOLED_SHARED_SOURCES}
    INCLUDE_DIRS
        "."
        "${STICKMON_ROOT}/src"
        "${STICKMON_ROOT}/firmware/common"
    REQUIRES ${STICKMON_MAIN_REQUIRES}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
if(STICKMON_ENABLE_CLAW)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STICKMON_HAS_CLAW=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STICKMON_HAS_CLAW=0)
endif()
if(STICKMON_ENABLE_DEBUG_FEATURES)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STICKMON_ENABLE_DEBUG_FEATURES=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STICKMON_ENABLE_DEBUG_FEATURES=0)
endif()
