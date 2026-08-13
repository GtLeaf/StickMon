cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET "esp32s3")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(stickmon_amoled_1_8_v1)

set(STICKMON_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(RESOURCE_STAGE_DIR "${CMAKE_BINARY_DIR}/stickmon_resources")
set(RESOURCE_ACTIVE "${STICKMON_ROOT}/data/active.json")
set(RESOURCE_MANIFEST "${STICKMON_ROOT}/data/packs/dev/manifest.json")
set(RESOURCE_BULBASAUR "${STICKMON_ROOT}/data/packs/dev/sprites/001.smonsp")
set(RESOURCE_ROOM "${STICKMON_ROOT}/data/packs/dev/rooms/standard.smonroom")
set(RESOURCE_MAPS "${STICKMON_ROOT}/data/packs/dev/game/maps.smonfx")
set(RESOURCE_UI "${STICKMON_ROOT}/data/packs/dev/game/ui.smonfx")

file(MAKE_DIRECTORY "${RESOURCE_STAGE_DIR}/packs/dev/sprites")
file(MAKE_DIRECTORY "${RESOURCE_STAGE_DIR}/packs/dev/rooms")
file(MAKE_DIRECTORY "${RESOURCE_STAGE_DIR}/packs/dev/game")
configure_file("${RESOURCE_ACTIVE}" "${RESOURCE_STAGE_DIR}/active.json" COPYONLY)
configure_file("${RESOURCE_MANIFEST}"
               "${RESOURCE_STAGE_DIR}/packs/dev/manifest.json" COPYONLY)
configure_file("${RESOURCE_BULBASAUR}"
               "${RESOURCE_STAGE_DIR}/packs/dev/sprites/001.smonsp" COPYONLY)
configure_file("${RESOURCE_ROOM}"
               "${RESOURCE_STAGE_DIR}/packs/dev/rooms/standard.smonroom" COPYONLY)
configure_file("${RESOURCE_MAPS}"
               "${RESOURCE_STAGE_DIR}/packs/dev/game/maps.smonfx" COPYONLY)
configure_file("${RESOURCE_UI}"
               "${RESOURCE_STAGE_DIR}/packs/dev/game/ui.smonfx" COPYONLY)

spiffs_create_partition_image(
    resources
    "${RESOURCE_STAGE_DIR}"
    FLASH_IN_PROJECT
    DEPENDS "${RESOURCE_ACTIVE}" "${RESOURCE_MANIFEST}"
            "${RESOURCE_BULBASAUR}" "${RESOURCE_ROOM}" "${RESOURCE_MAPS}"
            "${RESOURCE_UI}"
)
