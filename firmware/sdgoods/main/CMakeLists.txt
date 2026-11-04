# SPDX-License-Identifier: Apache-2.0
set(GAME_SOURCES)
foreach(DIR presentation assets core core/features hardware scenes game ui/common ui/compact_touch ui/round_touch)
    file(GLOB SOURCES "${STICKMON_ROOT}/src/${DIR}/*.cpp")
    list(APPEND GAME_SOURCES ${SOURCES})
endforeach()
list(FILTER GAME_SOURCES EXCLUDE REGEX "/ui/compact_touch/(HomeUi|MainMenuUi|FoodUi|MenuOverlayUi|SocialUi|HatchUi|ShopUi|BagUi|ComputerUi|TeamUi|RoomUi|ContactsUi|SettingsUi|StatusUi|MovesUi|DebugUi)[.]cpp$")
idf_component_register(SRCS "main.cpp" "SdgoodsPlatform.cpp" "RoomSession.cpp" "Audio.cpp" "Battery.cpp" "PeerRadio.cpp"
    "${STICKMON_ROOT}/src/platform/api/PlatformServices.cpp"
    "${STICKMON_ROOT}/firmware/common/Rgb565Transfer.cpp"
    "${STICKMON_ROOT}/firmware/common/Qmi8658.cpp"
    "${STICKMON_ROOT}/src/third_party/uzlib/tinflate.c" ${GAME_SOURCES}
    INCLUDE_DIRS "." "${STICKMON_ROOT}/src" "${STICKMON_ROOT}/firmware/common"
    REQUIRES sdgoods_panel esp_driver_gpio esp_driver_i2c esp_driver_ledc esp_timer esp_psram nvs_flash esp_driver_i2s esp_adc esp_wifi esp_netif esp_event)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    STICKMON_DISPLAY_WIDTH=360 STICKMON_DISPLAY_HEIGHT=360 STICKMON_ROOM_PIXEL_SCALE=1
    STICKMON_DEDICATED_POWER_BUTTON=1 STICKMON_TOUCH_INPUT=1
    STICKMON_UI_COMPACT_TOUCH=1 STICKMON_UI_ROUND_TOUCH=1
    STICKMON_UI_HEADER_INSET_X=60 STICKMON_UI_HEADER_TOP=32
    STICKMON_SDGOODS_RIGHT_TOUCH_COMPENSATION=$<BOOL:${STICKMON_SDGOODS_RIGHT_TOUCH_COMPENSATION}>
    STICKMON_ENABLE_DEBUG_FEATURES=$<BOOL:${STICKMON_ENABLE_DEBUG_FEATURES}>
    STICKMON_ENABLE_TRACE_LOGS=$<BOOL:${STICKMON_ENABLE_TRACE_LOGS}> STICKMON_HAS_CLAW=0)

idf_build_get_property(PACK_PYTHON PYTHON)
file(GLOB_RECURSE RESOURCE_INPUTS CONFIGURE_DEPENDS "${STICKMON_ROOT}/data/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${RESOURCE_INPUTS} "${STICKMON_ROOT}/tools/pack_sdgoods_resources.py"
    "${STICKMON_ROOT}/origin_asset/text/market_dialogue.json"
    "${STICKMON_ROOT}/tools/generate_market_dialogue.py")
execute_process(COMMAND "${PACK_PYTHON}" "${STICKMON_ROOT}/tools/generate_market_dialogue.py" --check
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${PACK_PYTHON}" "${STICKMON_ROOT}/tools/pack_sdgoods_resources.py"
    --output "${CMAKE_BINARY_DIR}/stickmon_resources.bin"
    RESULT_VARIABLE PACK_RESULT COMMAND_ERROR_IS_FATAL ANY)
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_BINARY_DIR}/stickmon_resources.bin" BINARY)
