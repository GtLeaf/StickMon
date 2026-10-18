# Explicit manifests are shared by board, native host and Web builds.
get_filename_component(STICKMON_AMOLED_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(STICKMON_AMOLED_MAIN "${STICKMON_AMOLED_ROOT}/firmware/amoled_1_8_v1/main")

function(stickmon_read_source_manifest manifest base output)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${manifest}")
    file(STRINGS "${manifest}" entries)
    set(sources)
    foreach(entry IN LISTS entries)
        string(STRIP "${entry}" entry)
        if(entry STREQUAL "" OR entry MATCHES "^#")
            continue()
        endif()
        if(IS_ABSOLUTE "${entry}" OR entry MATCHES "(^|/)[.][.](/|$)" OR
           NOT entry MATCHES "[.](cpp|c)$")
            message(FATAL_ERROR "Invalid source entry in ${manifest}: ${entry}")
        endif()
        set(source "${base}/${entry}")
        if(NOT EXISTS "${source}")
            message(FATAL_ERROR "Source listed in ${manifest} does not exist: ${source}")
        endif()
        if(source IN_LIST sources)
            message(FATAL_ERROR "Duplicate source in ${manifest}: ${entry}")
        endif()
        list(APPEND sources "${source}")
    endforeach()
    set(${output} ${sources} PARENT_SCOPE)
endfunction()

stickmon_read_source_manifest("${STICKMON_AMOLED_MAIN}/app/sources.txt"
    "${STICKMON_AMOLED_MAIN}" STICKMON_AMOLED_APP_SOURCES)
stickmon_read_source_manifest("${STICKMON_AMOLED_MAIN}/ui/sources.txt"
    "${STICKMON_AMOLED_MAIN}" STICKMON_AMOLED_UI_SOURCES)
stickmon_read_source_manifest("${CMAKE_CURRENT_LIST_DIR}/amoled_shared_sources.txt"
    "${STICKMON_AMOLED_ROOT}" STICKMON_AMOLED_SHARED_SOURCES)

set(all_sources ${STICKMON_AMOLED_APP_SOURCES} ${STICKMON_AMOLED_UI_SOURCES}
    ${STICKMON_AMOLED_SHARED_SOURCES})
list(LENGTH all_sources original_count)
list(REMOVE_DUPLICATES all_sources)
list(LENGTH all_sources unique_count)
if(NOT original_count EQUAL unique_count)
    message(FATAL_ERROR "Duplicate source across AMOLED manifests")
endif()
