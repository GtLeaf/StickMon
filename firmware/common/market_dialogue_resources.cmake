# Share the maintained dialogue resource between both AMOLED resource images.
set(RESOURCE_DIALOGUE "${STICKMON_ROOT}/data/packs/dev/text/market.smondialogue")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${STICKMON_ROOT}/origin_asset/text/market_dialogue.json"
    "${STICKMON_ROOT}/tools/generate_market_dialogue.py")
execute_process(COMMAND "${PYTHON}" "${STICKMON_ROOT}/tools/generate_market_dialogue.py" --check
    COMMAND_ERROR_IS_FATAL ANY)
file(MAKE_DIRECTORY "${RESOURCE_STAGE_DIR}/packs/dev/text")
configure_file("${RESOURCE_DIALOGUE}" "${RESOURCE_STAGE_DIR}/packs/dev/text/market.smondialogue" COPYONLY)
