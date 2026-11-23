cmake_minimum_required(VERSION 3.16)

option(STICKMON_ENABLE_CLAW "Build the ESP-Claw integration" ON)
option(STICKMON_ENABLE_DEBUG_FEATURES "Expose the StickMon debug menu" OFF)

set(STICKMON_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
include("${STICKMON_ROOT}/firmware/common/build_profile.cmake")
set(STICKMON_ESPCLAW_ROOT "$ENV{STICKMON_ESPCLAW_ROOT}")
if(NOT STICKMON_ESPCLAW_ROOT)
    set(STICKMON_ESPCLAW_ROOT "${STICKMON_ROOT}/../espclaw/esp-claw-master")
endif()
if(STICKMON_ENABLE_CLAW AND EXISTS "${STICKMON_ESPCLAW_ROOT}/components")
    set(EXTRA_COMPONENT_DIRS
        "${STICKMON_ROOT}/firmware/common/components"
        "${STICKMON_ESPCLAW_ROOT}/components/claw_modules/claw_utils"
        "${STICKMON_ESPCLAW_ROOT}/components/claw_modules/claw_core"
        "${STICKMON_ESPCLAW_ROOT}/components/claw_modules/claw_cap"
        "${STICKMON_ESPCLAW_ROOT}/components/claw_modules/claw_event_router"
        "${STICKMON_ESPCLAW_ROOT}/components/claw_modules/claw_manager"
        "${STICKMON_ESPCLAW_ROOT}/components/claw_modules/claw_memory"
        "${STICKMON_ESPCLAW_ROOT}/components/claw_modules/claw_skill"
        "${STICKMON_ESPCLAW_ROOT}/components/claw_capabilities/cap_im_platform"
        "${STICKMON_ESPCLAW_ROOT}/components/claw_capabilities/cap_llm_config"
        "${STICKMON_ESPCLAW_ROOT}/components/claw_capabilities/cap_skill_mgr"
        "${STICKMON_ESPCLAW_ROOT}/components/common/http_reuse"
    )
elseif(STICKMON_ENABLE_CLAW)
    message(WARNING "ESP-Claw source not found at ${STICKMON_ESPCLAW_ROOT}; remote chat is unavailable")
endif()

set(IDF_TARGET "esp32s3")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Keep dependency resolution isolated per build variant. Claw's local
# component paths are machine-specific and must not be committed.
idf_build_set_property(DEPENDENCIES_LOCK
                       "${CMAKE_BINARY_DIR}/dependencies.lock")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/ota-generated")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${STICKMON_ROOT}/tools/generate_ota_build.py"
    "${STICKMON_ROOT}/src/core/SaveCodec.h"
    "${STICKMON_ROOT}/src/game/GameState.h")
project(stickmon_amoled_1_8_v2)
if(NOT CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE)
    message(FATAL_ERROR "V2 OTA requires CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y; use tools/build_amoled_variant.sh or update cached sdkconfig")
endif()
if(NOT CONFIG_FATFS_LFN_HEAP AND NOT CONFIG_FATFS_LFN_STACK)
    message(FATAL_ERROR "V2 photos require FAT long file names; use tools/build_amoled_variant.sh or enable CONFIG_FATFS_LFN_HEAP in sdkconfig")
endif()
# The published photo slot also needs space for the temporary .tmp suffix.
if(CONFIG_FATFS_MAX_LFN LESS 15)
    message(FATAL_ERROR "V2 photos require CONFIG_FATFS_MAX_LFN of at least 15; update cached sdkconfig")
endif()

# ESP-IDF appends its default C++ standard during project setup. Append the
# project standard afterward so the final compiler option is unambiguous.
idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17" APPEND)

set(RESOURCE_STAGE_DIR "${CMAKE_BINARY_DIR}/stickmon_resources")
set(RESOURCE_ACTIVE "${STICKMON_ROOT}/data/active.json")
set(RESOURCE_MANIFEST "${STICKMON_ROOT}/data/packs/dev/manifest.json")
set(RESOURCE_SPRITES_DIR "${STICKMON_ROOT}/data/packs/dev/sprites")
set(RESOURCE_ROOM "${STICKMON_ROOT}/data/packs/dev/rooms/standard.smonroom")
set(WALLPAPER_CATALOG "${STICKMON_ROOT}/origin_asset/room/wallpapers/catalog.json")
set(WALLPAPER_LISTER "${STICKMON_ROOT}/tools/wallpaper_catalog.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${WALLPAPER_CATALOG}" "${WALLPAPER_LISTER}"
    "${STICKMON_ROOT}/src/assets/WallpaperCatalog.inc")
execute_process(
    COMMAND "${PYTHON}" "${WALLPAPER_LISTER}" --catalog "${WALLPAPER_CATALOG}"
    OUTPUT_VARIABLE WALLPAPER_PATHS OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE WALLPAPER_CHECK ERROR_VARIABLE WALLPAPER_ERROR)
if(NOT WALLPAPER_CHECK EQUAL 0)
    message(FATAL_ERROR "${WALLPAPER_ERROR}")
endif()
string(REPLACE "\n" ";" RESOURCE_WALLPAPERS "${WALLPAPER_PATHS}")
set(RESOURCE_MAPS "${STICKMON_ROOT}/data/packs/dev/game/maps.smonfx")
set(RESOURCE_UI "${STICKMON_ROOT}/data/packs/dev/game/ui_amoled.smonfx")
set(RESOURCE_BATTLE "${STICKMON_ROOT}/data/packs/dev/game/battle.smonfx")
set(RESOURCE_HATCH "${STICKMON_ROOT}/data/packs/dev/game/hatch.smonfx")
set(RESOURCE_FONTS_DIR "${STICKMON_ROOT}/data/packs/dev/fonts")
set(RESOURCE_AUDIO_DIR "${STICKMON_ROOT}/data/packs/dev/audio")
set(RESOURCE_CRIES_DIR "${STICKMON_ROOT}/data/packs/dev/cries")

# Kind ids can change when shared assets are added. Never ship a stale native
# UI pack: the runtime rejects mismatched packs and falls back to drawn icons.
set(RESOURCE_UI_VALIDATOR "${STICKMON_ROOT}/tools/validate_amoled_ui_assets.py")
execute_process(
    COMMAND "${PYTHON}" "${RESOURCE_UI_VALIDATOR}"
            "${STICKMON_ROOT}/data/packs/dev/game/ui.smonfx" "${RESOURCE_UI}"
    RESULT_VARIABLE RESOURCE_UI_CHECK
    ERROR_VARIABLE RESOURCE_UI_ERROR
)
if(NOT RESOURCE_UI_CHECK EQUAL 0)
    message(FATAL_ERROR "${RESOURCE_UI_ERROR}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${RESOURCE_UI_VALIDATOR}" "${STICKMON_ROOT}/src/assets/GameAssets.h"
    "${STICKMON_ROOT}/data/packs/dev/game/ui.smonfx")

file(GLOB RESOURCE_FONT_FILES CONFIGURE_DEPENDS
     "${RESOURCE_FONTS_DIR}/*.smonfont")
# Native AMOLED uses bitmap fonts only; A4 coverage fonts belong to round UI.
list(FILTER RESOURCE_FONT_FILES EXCLUDE REGEX "/round-zh(16|20)-a4[.]smonfont$")
file(GLOB RESOURCE_SPRITE_FILES CONFIGURE_DEPENDS
     "${RESOURCE_SPRITES_DIR}/*.smonsp")
file(GLOB RESOURCE_AUDIO_FILES CONFIGURE_DEPENDS
     "${RESOURCE_AUDIO_DIR}/*.smonaudio")
file(GLOB RESOURCE_CRY_FILES CONFIGURE_DEPENDS
     "${RESOURCE_CRIES_DIR}/*.smoncry")
set(RESOURCE_MENU "${STICKMON_ROOT}/data/packs/dev/ui/menu.smonmenu")
set(RESOURCE_LAYOUT_FILES)
foreach(LAYOUT_ID RANGE 10 20)
    set(LAYOUT_FILE "${STICKMON_ROOT}/data/packs/dev/catalog/${LAYOUT_ID}.smondata")
    if(NOT EXISTS "${LAYOUT_FILE}")
        message(FATAL_ERROR "Missing map table: run tools/generate_map_layout_resources.py")
    endif()
    list(APPEND RESOURCE_LAYOUT_FILES "${LAYOUT_FILE}")
endforeach()
if(NOT EXISTS "${RESOURCE_MENU}")
    message(FATAL_ERROR "Missing menu icons: run tools/generate_menu_icon_pack.py")
endif()

if(NOT RESOURCE_FONT_FILES)
    message(FATAL_ERROR "StickMon font resources are missing")
endif()
if(NOT RESOURCE_SPRITE_FILES)
    message(FATAL_ERROR "StickMon sprite resources are missing")
endif()

file(REMOVE_RECURSE "${RESOURCE_STAGE_DIR}")
include("${STICKMON_ROOT}/firmware/common/market_dialogue_resources.cmake")
file(MAKE_DIRECTORY "${RESOURCE_STAGE_DIR}/packs/dev/sprites")
file(MAKE_DIRECTORY "${RESOURCE_STAGE_DIR}/packs/dev/ui" "${RESOURCE_STAGE_DIR}/packs/dev/catalog")
configure_file("${RESOURCE_MENU}" "${RESOURCE_STAGE_DIR}/packs/dev/ui/menu.smonmenu" COPYONLY)
foreach(LAYOUT_FILE IN LISTS RESOURCE_LAYOUT_FILES)
    get_filename_component(LAYOUT_NAME "${LAYOUT_FILE}" NAME)
    configure_file("${LAYOUT_FILE}" "${RESOURCE_STAGE_DIR}/packs/dev/catalog/${LAYOUT_NAME}" COPYONLY)
endforeach()
file(MAKE_DIRECTORY "${RESOURCE_STAGE_DIR}/packs/dev/rooms")
file(MAKE_DIRECTORY "${RESOURCE_STAGE_DIR}/packs/dev/game")
file(MAKE_DIRECTORY "${RESOURCE_STAGE_DIR}/packs/dev/fonts")
file(MAKE_DIRECTORY "${RESOURCE_STAGE_DIR}/packs/dev/audio")
file(MAKE_DIRECTORY "${RESOURCE_STAGE_DIR}/packs/dev/cries")
configure_file("${RESOURCE_ACTIVE}" "${RESOURCE_STAGE_DIR}/active.json" COPYONLY)
configure_file("${RESOURCE_MANIFEST}"
               "${RESOURCE_STAGE_DIR}/packs/dev/manifest.json" COPYONLY)
foreach(RESOURCE_SPRITE_FILE IN LISTS RESOURCE_SPRITE_FILES)
    get_filename_component(RESOURCE_SPRITE_NAME "${RESOURCE_SPRITE_FILE}" NAME)
    configure_file("${RESOURCE_SPRITE_FILE}"
                   "${RESOURCE_STAGE_DIR}/packs/dev/sprites/${RESOURCE_SPRITE_NAME}"
                   COPYONLY)
endforeach()
configure_file("${RESOURCE_ROOM}"
               "${RESOURCE_STAGE_DIR}/packs/dev/rooms/standard.smonroom" COPYONLY)
foreach(WALLPAPER_FILE IN LISTS RESOURCE_WALLPAPERS)
    get_filename_component(WALLPAPER_NAME "${WALLPAPER_FILE}" NAME)
    configure_file("${WALLPAPER_FILE}" "${RESOURCE_STAGE_DIR}/packs/dev/rooms/${WALLPAPER_NAME}" COPYONLY)
endforeach()
# Layered rooms and reusable furniture are currently a Debug preview feature.
set(RESOURCE_DEBUG_ROOMS)
if(STICKMON_ENABLE_DEBUG_FEATURES)
    set(RESOURCE_LAYERED_ROOT "${STICKMON_ROOT}/origin_asset/generated/room/layered")
    file(GLOB RESOURCE_LAYERED_ROOMS CONFIGURE_DEPENDS "${RESOURCE_LAYERED_ROOT}/rooms/*.smonroom")
    file(GLOB RESOURCE_FURNITURE_FILES CONFIGURE_DEPENDS "${RESOURCE_LAYERED_ROOT}/furniture/*.smonfurn")
    if(NOT EXISTS "${RESOURCE_LAYERED_ROOT}/rooms/standard.smonroom" OR NOT RESOURCE_FURNITURE_FILES)
        message(FATAL_ERROR "Generate layered rooms with tools/generate_room_layers.py first")
    endif()
    file(MAKE_DIRECTORY "${RESOURCE_STAGE_DIR}/packs/dev/furniture")
    foreach(RESOURCE_LAYERED_ROOM IN LISTS RESOURCE_LAYERED_ROOMS)
        get_filename_component(RESOURCE_NAME "${RESOURCE_LAYERED_ROOM}" NAME)
        configure_file("${RESOURCE_LAYERED_ROOM}"
            "${RESOURCE_STAGE_DIR}/packs/dev/rooms/${RESOURCE_NAME}" COPYONLY)
    endforeach()
    foreach(RESOURCE_FURNITURE_FILE IN LISTS RESOURCE_FURNITURE_FILES)
        get_filename_component(RESOURCE_NAME "${RESOURCE_FURNITURE_FILE}" NAME)
        configure_file("${RESOURCE_FURNITURE_FILE}"
            "${RESOURCE_STAGE_DIR}/packs/dev/furniture/${RESOURCE_NAME}" COPYONLY)
    endforeach()
    set(RESOURCE_DEBUG_ROOMS ${RESOURCE_LAYERED_ROOMS} ${RESOURCE_FURNITURE_FILES})
endif()
configure_file("${RESOURCE_MAPS}"
               "${RESOURCE_STAGE_DIR}/packs/dev/game/maps.smonfx" COPYONLY)
configure_file("${RESOURCE_UI}"
               "${RESOURCE_STAGE_DIR}/packs/dev/game/ui.smonfx" COPYONLY)
configure_file("${RESOURCE_BATTLE}"
               "${RESOURCE_STAGE_DIR}/packs/dev/game/battle.smonfx" COPYONLY)
configure_file("${RESOURCE_HATCH}"
               "${RESOURCE_STAGE_DIR}/packs/dev/game/hatch.smonfx" COPYONLY)
foreach(RESOURCE_FONT_FILE IN LISTS RESOURCE_FONT_FILES)
    get_filename_component(RESOURCE_FONT_NAME "${RESOURCE_FONT_FILE}" NAME)
    configure_file("${RESOURCE_FONT_FILE}"
                   "${RESOURCE_STAGE_DIR}/packs/dev/fonts/${RESOURCE_FONT_NAME}"
                   COPYONLY)
endforeach()
foreach(RESOURCE_AUDIO_FILE IN LISTS RESOURCE_AUDIO_FILES)
    get_filename_component(RESOURCE_AUDIO_NAME
                           "${RESOURCE_AUDIO_FILE}" NAME)
    configure_file("${RESOURCE_AUDIO_FILE}"
                   "${RESOURCE_STAGE_DIR}/packs/dev/audio/${RESOURCE_AUDIO_NAME}"
                   COPYONLY)
endforeach()
foreach(RESOURCE_CRY_FILE IN LISTS RESOURCE_CRY_FILES)
    get_filename_component(RESOURCE_CRY_NAME "${RESOURCE_CRY_FILE}" NAME)
    configure_file("${RESOURCE_CRY_FILE}"
                   "${RESOURCE_STAGE_DIR}/packs/dev/cries/${RESOURCE_CRY_NAME}"
                   COPYONLY)
endforeach()

spiffs_create_partition_image(
    resources
    "${RESOURCE_STAGE_DIR}"
    FLASH_IN_PROJECT
    DEPENDS "${RESOURCE_ACTIVE}" "${RESOURCE_MANIFEST}" "${RESOURCE_MENU}" ${RESOURCE_LAYOUT_FILES}
            ${RESOURCE_SPRITE_FILES} "${RESOURCE_ROOM}" ${RESOURCE_WALLPAPERS} ${RESOURCE_DEBUG_ROOMS} "${RESOURCE_MAPS}"
            "${RESOURCE_UI}" "${RESOURCE_BATTLE}" "${RESOURCE_HATCH}"
            ${RESOURCE_FONT_FILES} ${RESOURCE_AUDIO_FILES}
            ${RESOURCE_CRY_FILES} "${RESOURCE_DIALOGUE}"
)

# OTA identifies the deployed resource file set, not a mutable pack version.
set(STICKMON_OTA_ENDPOINT "" CACHE STRING "HTTPS flasher origin for OTA")
if(STICKMON_ENABLE_CLAW)
    set(OTA_FLAVOR claw)
else()
    set(OTA_FLAVOR lite)
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${STICKMON_ROOT}/tools/generate_ota_build.py"
    "${CMAKE_CURRENT_LIST_DIR}/ota/version.json"
    "${CMAKE_CURRENT_LIST_DIR}/ota/public-key.pem")
execute_process(COMMAND "${PYTHON}" "${STICKMON_ROOT}/tools/generate_ota_build.py"
    "${RESOURCE_STAGE_DIR}" "${CMAKE_BINARY_DIR}/ota-generated"
    "${CMAKE_CURRENT_LIST_DIR}/ota/version.json" "${OTA_FLAVOR}"
    "${STICKMON_OTA_ENDPOINT}" "${CMAKE_CURRENT_LIST_DIR}/ota/public-key.pem"
    "${STICKMON_ENABLE_DEBUG_FEATURES}"
    COMMAND_ERROR_IS_FATAL ANY)
