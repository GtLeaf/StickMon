cmake_minimum_required(VERSION 3.16)

option(STICKMON_ENABLE_CLAW "Build the ESP-Claw integration" ON)

set(STICKMON_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
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
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Keep dependency resolution isolated per build variant. Claw's local
# component paths are machine-specific and must not be committed.
idf_build_set_property(DEPENDENCIES_LOCK
                       "${CMAKE_BINARY_DIR}/dependencies.lock")
project(stickmon_amoled_1_75c)

set(RESOURCE_STAGE_DIR "${CMAKE_BINARY_DIR}/stickmon_resources")
set(RESOURCE_ACTIVE "${STICKMON_ROOT}/data/active.json")
set(RESOURCE_MANIFEST "${STICKMON_ROOT}/data/packs/dev/manifest.json")
set(RESOURCE_SPRITES_DIR "${STICKMON_ROOT}/data/packs/dev/sprites")
set(RESOURCE_ROOM "${STICKMON_ROOT}/data/packs/dev/rooms/standard.smonroom")
set(RESOURCE_MAPS "${STICKMON_ROOT}/data/packs/dev/game/maps.smonfx")
set(RESOURCE_UI "${STICKMON_ROOT}/data/packs/dev/game/ui.smonfx")
set(RESOURCE_BATTLE "${STICKMON_ROOT}/data/packs/dev/game/battle.smonfx")
set(RESOURCE_HATCH "${STICKMON_ROOT}/data/packs/dev/game/hatch.smonfx")
set(RESOURCE_FONTS_DIR "${STICKMON_ROOT}/data/packs/dev/fonts")
set(RESOURCE_AUDIO_DIR "${STICKMON_ROOT}/data/packs/dev/audio")
set(RESOURCE_CRIES_DIR "${STICKMON_ROOT}/data/packs/dev/cries")

file(GLOB RESOURCE_FONT_FILES CONFIGURE_DEPENDS
     "${RESOURCE_FONTS_DIR}/*.smonfont")
file(GLOB RESOURCE_SPRITE_FILES CONFIGURE_DEPENDS
     "${RESOURCE_SPRITES_DIR}/*.smonsp")
file(GLOB RESOURCE_AUDIO_FILES CONFIGURE_DEPENDS
     "${RESOURCE_AUDIO_DIR}/*.smonaudio")
file(GLOB RESOURCE_CRY_FILES CONFIGURE_DEPENDS
     "${RESOURCE_CRIES_DIR}/*.smoncry")

if(NOT RESOURCE_FONT_FILES)
    message(FATAL_ERROR "StickMon font resources are missing")
endif()
if(NOT RESOURCE_SPRITE_FILES)
    message(FATAL_ERROR "StickMon sprite resources are missing")
endif()

file(MAKE_DIRECTORY "${RESOURCE_STAGE_DIR}/packs/dev/sprites")
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
    DEPENDS "${RESOURCE_ACTIVE}" "${RESOURCE_MANIFEST}"
            ${RESOURCE_SPRITE_FILES} "${RESOURCE_ROOM}" "${RESOURCE_MAPS}"
            "${RESOURCE_UI}" "${RESOURCE_BATTLE}" "${RESOURCE_HATCH}"
            ${RESOURCE_FONT_FILES} ${RESOURCE_AUDIO_FILES}
            ${RESOURCE_CRY_FILES}
)
