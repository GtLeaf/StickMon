# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)
option(STICKMON_ENABLE_DEBUG_FEATURES "Expose the StickMon debug menu" OFF)
option(STICKMON_ENABLE_TRACE_LOGS "Enable StickMon trace logs in debug builds" OFF)
option(STICKMON_ENABLE_ROOM_DIAGNOSTICS "Log room state without enabling debug features" OFF)
option(STICKMON_SDGOODS_RIGHT_TOUCH_COMPENSATION "Try progressive right-side touch correction without a saved calibration" ON)
option(STICKMON_SDGOODS_FULL_STORAGE "Use the full 32 MiB layout with 24 MiB resources" OFF)
set(IDF_TARGET esp32s3)
set(STICKMON_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(STICKMON_SDGOODS_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../sdgoods-esp32s3"
    CACHE PATH "Checkout of the SDGOODS ESP32-S3 SDK")
get_filename_component(STICKMON_SDGOODS_SDK_ROOT "${STICKMON_SDGOODS_SDK_ROOT}" ABSOLUTE)
if(NOT EXISTS "${STICKMON_SDGOODS_SDK_ROOT}/components/sdgoods_board/CMakeLists.txt")
    message(FATAL_ERROR "Set STICKMON_SDGOODS_SDK_ROOT to a SDGOODS SDK checkout")
endif()
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components/sdgoods_panel")
# Build the game plus its native panel component; do not discover the SDK launcher.
set(COMPONENTS main)
configure_file("${STICKMON_SDGOODS_SDK_ROOT}/LICENSE" "${CMAKE_CURRENT_BINARY_DIR}/SDGOODS-LICENSE" COPYONLY)
configure_file("${STICKMON_SDGOODS_SDK_ROOT}/NOTICE" "${CMAKE_CURRENT_BINARY_DIR}/SDGOODS-NOTICE" COPYONLY)
# Standalone game layout; the SDK launcher is not part of this application.
if(STICKMON_SDGOODS_FULL_STORAGE)
    set(STICKMON_SDGOODS_PARTITIONS "${CMAKE_CURRENT_LIST_DIR}/partitions-32m.csv")
    set(STICKMON_SDGOODS_RESOURCE_LAYOUT "32m")
else()
    set(STICKMON_SDGOODS_PARTITIONS "${CMAKE_CURRENT_LIST_DIR}/partitions.csv")
    set(STICKMON_SDGOODS_RESOURCE_LAYOUT "16m")
endif()
configure_file("${STICKMON_SDGOODS_PARTITIONS}"
    "${CMAKE_CURRENT_BINARY_DIR}/sdgoods-partitions.csv" COPYONLY)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sdkconfig.partition"
    "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"${CMAKE_CURRENT_BINARY_DIR}/sdgoods-partitions.csv\"\n")
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults;${CMAKE_CURRENT_BINARY_DIR}/sdkconfig.partition")
# Keep each build independent of the earlier BSP/LVGL configuration and its paths.
set(SDKCONFIG "${CMAKE_CURRENT_BINARY_DIR}/sdkconfig")
include("${STICKMON_ROOT}/firmware/common/build_profile.cmake")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(stickmon_sdgoods)
idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17" APPEND)
