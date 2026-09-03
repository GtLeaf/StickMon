set(STICKMON_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

set(STICKMON_MAIN_REQUIRES
    esp_lcd
    esp_codec_dev
    esp_event
    esp_netif
    esp_psram
    esp_system
    esp_timer
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
        "../../amoled_1_8_v1/main/AmoledApp.cpp"
        "AmoledPlatform.cpp"
        "../../amoled_1_8_v1/main/HomeScreen.cpp"
        "TouchInput.cpp"
        "${STICKMON_ROOT}/src/assets/FontFallbackCN.cpp"
        "${STICKMON_ROOT}/src/assets/GameAssets.cpp"
        "${STICKMON_ROOT}/src/assets/HudAssets.cpp"
        "${STICKMON_ROOT}/src/assets/MenuAssets.cpp"
        "${STICKMON_ROOT}/src/assets/PokemonSprites.cpp"
        "${STICKMON_ROOT}/src/core/DeflateDecoder.cpp"
        "${STICKMON_ROOT}/src/core/AudioManager.cpp"
        "${STICKMON_ROOT}/src/core/CryPlayer.cpp"
        "${STICKMON_ROOT}/src/core/FontResource.cpp"
        "${STICKMON_ROOT}/src/core/GameClockService.cpp"
        "${STICKMON_ROOT}/src/core/ResourceFS.cpp"
        "${STICKMON_ROOT}/src/core/ResourcePack.cpp"
        "${STICKMON_ROOT}/src/core/RoomMovementArea.cpp"
        "${STICKMON_ROOT}/src/core/RoomRenderer.cpp"
        "${STICKMON_ROOT}/src/core/RoomResource.cpp"
        "${STICKMON_ROOT}/src/core/SaveCodec.cpp"
        "${STICKMON_ROOT}/src/core/SaveManager.cpp"
        "${STICKMON_ROOT}/src/core/VisitSessionService.cpp"
        "${STICKMON_ROOT}/src/game/HomeCare.cpp"
        "${STICKMON_ROOT}/src/game/HomeChase.cpp"
        "${STICKMON_ROOT}/src/game/BathService.cpp"
        "${STICKMON_ROOT}/src/game/BattleSystem.cpp"
        "${STICKMON_ROOT}/src/game/BattleTurnController.cpp"
        "${STICKMON_ROOT}/src/game/ExperienceService.cpp"
        "${STICKMON_ROOT}/src/game/FriendshipService.cpp"
        "${STICKMON_ROOT}/src/game/FriendshipSystem.cpp"
        "${STICKMON_ROOT}/src/game/ItemInventory.cpp"
        "${STICKMON_ROOT}/src/game/MonsterFactory.cpp"
        "${STICKMON_ROOT}/src/game/MoveManagementService.cpp"
        "${STICKMON_ROOT}/src/game/ShopService.cpp"
        "${STICKMON_ROOT}/src/game/ExploreMapGenerator.cpp"
        "${STICKMON_ROOT}/src/game/GameRandom.cpp"
        "${STICKMON_ROOT}/src/game/MonsterMind.cpp"
        "${STICKMON_ROOT}/src/game/Species.cpp"
        "${STICKMON_ROOT}/src/hardware/EspNowLink.cpp"
        "${STICKMON_ROOT}/src/platform/api/PlatformServices.cpp"
        "${STICKMON_ROOT}/src/presentation/Canvas565.cpp"
        "${STICKMON_ROOT}/src/presentation/HudRenderer.cpp"
        "${STICKMON_ROOT}/src/presentation/PixelRenderer.cpp"
        "${STICKMON_ROOT}/src/third_party/uzlib/tinflate.c"
    INCLUDE_DIRS
        "."
        "../../amoled_1_8_v1/main"
        "${STICKMON_ROOT}/src"
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
