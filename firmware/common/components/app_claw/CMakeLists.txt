set(STICKMON_ESPCLAW_ROOT "$ENV{STICKMON_ESPCLAW_ROOT}")
if(NOT STICKMON_ESPCLAW_ROOT)
    set(STICKMON_ESPCLAW_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../../espclaw/esp-claw-master")
endif()

set(APP_CLAW_ROOT "${STICKMON_ESPCLAW_ROOT}/components/common/app_claw")

idf_component_register(
    SRCS
        "${APP_CLAW_ROOT}/app_claw.c"
        "${APP_CLAW_ROOT}/app_capabilities.c"
        "${APP_CLAW_ROOT}/app_lua_modules.c"
    INCLUDE_DIRS
        "${APP_CLAW_ROOT}/include"
    REQUIRES
        cap_im_platform
        cap_llm_config
        cap_skill_mgr
        claw_cap
        claw_core
        claw_event_router
        claw_manager
        claw_memory
        claw_skill
        claw_utils
    PRIV_REQUIRES
        console
)
