set(STICKMON_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(GAME_SOURCES)
foreach(DIR presentation assets core hardware scenes game)
    file(GLOB SOURCES "${STICKMON_ROOT}/src/${DIR}/*.cpp")
    list(APPEND GAME_SOURCES ${SOURCES})
endforeach()
idf_component_register(SRCS "main.cpp" "OstbPlatform.cpp" "OstbTouch.cpp"
    "${STICKMON_ROOT}/firmware/common/Rgb565Transfer.cpp"
    "${STICKMON_ROOT}/src/platform/api/PlatformServices.cpp"
    "${STICKMON_ROOT}/src/third_party/uzlib/tinflate.c" ${GAME_SOURCES}
    INCLUDE_DIRS "." "${STICKMON_ROOT}/src" "${STICKMON_ROOT}/firmware/common"
    REQUIRES esp_lcd esp_driver_spi esp_driver_gpio esp_driver_ledc esp_driver_i2c
        esp_driver_i2s esp_adc esp_timer esp_psram nvs_flash spiffs esp_wifi esp_event esp_netif esp_codec_dev)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_definitions(${COMPONENT_LIB} PUBLIC STICKMON_DISPLAY_WIDTH=296
    STICKMON_DISPLAY_HEIGHT=240 STICKMON_ROOM_PIXEL_SCALE=1 STICKMON_DEDICATED_POWER_BUTTON=1 STICKMON_TOUCH_INPUT=1
    STICKMON_ENABLE_DEBUG_FEATURES=0 STICKMON_ENABLE_TRACE_LOGS=0 STICKMON_HAS_CLAW=0)
