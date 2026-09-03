set(STICKMON_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")

idf_component_register(
    SRCS
        "${STICKMON_ROOT}/src/brain/BrainBridge.cpp"
        "${STICKMON_ROOT}/src/brain/ClawStatusLog.cpp"
        "${STICKMON_ROOT}/src/brain/StickmonCapability.cpp"
        "${STICKMON_ROOT}/src/brain/StickmonClawRuntime.cpp"
        "${STICKMON_ROOT}/src/presentation/QrCodeGen.cpp"
    INCLUDE_DIRS
        "${STICKMON_ROOT}/src"
    REQUIRES
        app_claw
        cap_im_platform
        claw_cap
        claw_core
        cjson
        esp_event
        esp_http_server
        esp_netif
        spiffs
        fatfs
        esp_wifi
        freertos
        nvs_flash
        wear_levelling
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_definitions(${COMPONENT_LIB} PRIVATE STICKMON_CLAW_INTERNAL=1)
