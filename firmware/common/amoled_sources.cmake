# Keep board builds and host rendering on the same explicit UI source list.
get_filename_component(STICKMON_AMOLED_MAIN
    "${CMAKE_CURRENT_LIST_DIR}/../amoled_1_8_v1/main" ABSOLUTE)
set(STICKMON_AMOLED_UI_MANIFEST "${STICKMON_AMOLED_MAIN}/ui/sources.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${STICKMON_AMOLED_UI_MANIFEST}")
file(STRINGS "${STICKMON_AMOLED_UI_MANIFEST}" STICKMON_AMOLED_UI_ENTRIES)
set(STICKMON_AMOLED_UI_SOURCES)
foreach(entry IN LISTS STICKMON_AMOLED_UI_ENTRIES)
    string(STRIP "${entry}" entry)
    if(NOT entry STREQUAL "" AND NOT entry MATCHES "^#")
        list(APPEND STICKMON_AMOLED_UI_SOURCES "${STICKMON_AMOLED_MAIN}/${entry}")
    endif()
endforeach()
set(STICKMON_AMOLED_APP_SOURCE "${STICKMON_AMOLED_MAIN}/AmoledApp.cpp")
