# Minimal assert()-based CTest harness placeholder.
# TODO: replace with Unity/CMock (or equivalent qualified framework) before
# this is used as SIL 3/4 verification evidence - see ADR-001 section 7.

function(safeapi_add_test name source lib)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ${lib})
    safeapi_set_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# ADR-023: each test links whichever of the 4 consolidated libraries
# (core/oal/channels/appmanager) covers the module under test - PUBLIC
# linking inside those libraries means one name still pulls in
# everything transitively required, exactly as the old fine-grained set
# of individual safeapi::<feature> targets did.
#
# ADR-024: each optional module's test is now gated behind the same
# SAFEAPI_ENABLE_* option that gates the module's own source file, so
# `cmake -DSAFEAPI_ENABLE_WATCHDOG=OFF` doesn't leave a dangling test that
# tries to link a library the build no longer produces.

# CORE - always built, no toggle.
safeapi_add_test(test_sapi_status         status/test_sapi_status.c            safeapi::core)
safeapi_add_test(test_sapi_buffer         buffer/test_sapi_buffer.c            safeapi::core)
safeapi_add_test(test_sapi_cast           cast/test_sapi_cast.c                safeapi::core)
safeapi_add_test(test_sapi_safestate      safestate/test_sapi_safestate.c      safeapi::core)
safeapi_add_test(test_sapi_safety_violation safestate/test_sapi_safety_violation.c safeapi::core)
safeapi_add_test(test_sapi_string         string/test_sapi_string.c            safeapi::core)
safeapi_add_test(test_sapi_lifecycle      lifecycle/test_sapi_lifecycle.c      safeapi::core)

# OAL
if(SAFEAPI_ENABLE_TIMER)
    safeapi_add_test(test_sapi_timer      timer/test_sapi_timer.c              safeapi::oal)
endif()
if(SAFEAPI_ENABLE_NVM)
    safeapi_add_test(test_sapi_nvm        nvm/test_sapi_nvm.c                  safeapi::oal)
endif()
if(SAFEAPI_ENABLE_MEMORY)
    safeapi_add_test(test_sapi_memory     memory/test_sapi_memory.c            safeapi::oal)
    safeapi_add_test(test_sapi_safe_ptr   memory/test_sapi_safe_ptr.c          safeapi::oal)
endif()
if(SAFEAPI_ENABLE_TASK)
    safeapi_add_test(test_sapi_task       task/test_sapi_task.c                safeapi::oal)
endif()
if(SAFEAPI_ENABLE_IPC)
    safeapi_add_test(test_sapi_ipc        ipc/test_sapi_ipc.c                  safeapi::oal)
endif()
if(SAFEAPI_ENABLE_REBOOT)
    safeapi_add_test(test_sapi_reboot     reboot/test_sapi_reboot.c            safeapi::oal)
endif()
if(SAFEAPI_ENABLE_PLATFORM)
    safeapi_add_test(test_sapi_platform   platform/test_sapi_platform.c        safeapi::oal)
endif()
if(SAFEAPI_ENABLE_NETLINK)
    safeapi_add_test(test_sapi_netlink    netlink/test_sapi_netlink.c          safeapi::oal)
endif()
if(SAFEAPI_ENABLE_FLOW)
    safeapi_add_test(test_sapi_flow       flow/test_sapi_flow.c                safeapi::oal)
endif()
if(SAFEAPI_ENABLE_LOG)
    safeapi_add_test(test_sapi_log        log/test_sapi_log.c                  safeapi::oal)
endif()
if(SAFEAPI_ENABLE_WATCHDOG)
    safeapi_add_test(test_sapi_watchdog   watchdog/test_sapi_watchdog.c        safeapi::oal)
endif()

# Channels
if(SAFEAPI_ENABLE_CHECKSUM)
    safeapi_add_test(test_sapi_checksum   checksum/test_sapi_checksum.c        safeapi::channels)
endif()
if(SAFEAPI_ENABLE_CHANNEL_LINK)
    safeapi_add_test(test_sapi_channel channel_link/test_sapi_channel.c safeapi::channels)
endif()
if(SAFEAPI_ENABLE_CHANNEL_LINK AND SAFEAPI_ENABLE_FLOW)
    safeapi_add_test(test_sapi_channel_service_flow_backend channel_service/test_sapi_channel_service_flow_backend.c safeapi::channels)
endif()
if(SAFEAPI_ENABLE_VOTER)
    safeapi_add_test(test_sapi_voter       voter/test_sapi_voter.c              safeapi::channels)
endif()
if(SAFEAPI_ENABLE_VOTER AND SAFEAPI_ENABLE_REDUNDANCY_CONFIG)
    safeapi_add_test(test_sapi_redundancy_config redundancy_config/test_sapi_redundancy_config.c safeapi::channels)
endif()
if(SAFEAPI_ENABLE_STATE_TRANSFER)
    safeapi_add_test(test_sapi_state_transfer state_transfer/test_sapi_state_transfer.c safeapi::channels)
endif()
if(SAFEAPI_ENABLE_CROSS_COMPARATOR)
    safeapi_add_test(test_sapi_cross_comparator cross_comparator/test_sapi_cross_comparator.c safeapi::channels)
endif()
if(SAFEAPI_ENABLE_CLOCKSYNC)
    safeapi_add_test(test_sapi_clocksync  clocksync/test_sapi_clocksync.c      safeapi::channels)
endif()
if(SAFEAPI_ENABLE_CHECKPOINT)
    safeapi_add_test(test_sapi_checkpoint checkpoint/test_sapi_checkpoint.c    safeapi::channels)
endif()
if(SAFEAPI_ENABLE_DUAL)
    safeapi_add_test(test_sapi_dual_types      dual/test_sapi_dual_types.c      safeapi::channels)
    safeapi_add_test(test_sapi_dual_msgchannel dual/test_sapi_dual_msgchannel.c safeapi::channels)
    safeapi_add_test(test_sapi_dual_channel    dual/test_sapi_dual_channel.c    safeapi::channels)
    safeapi_add_test(test_sapi_dual_negotiator dual/test_sapi_dual_negotiator.c safeapi::channels)
endif()
if(SAFEAPI_ENABLE_SAFECHANNEL)
    safeapi_add_test(test_sapi_safechannel     safechannel/test_sapi_safechannel.c safeapi::channels)
endif()

# App layer
if(SAFEAPI_ENABLE_APPMANAGER)
    safeapi_add_test(test_sapi_appmanager appmanager/test_sapi_appmanager.c    safeapi::appmanager)
endif()

# posix_backend (and its tests) moved to the safeAPIRBC2oo2 project - an
# OAL backend is an integrator-supplied implementation (ADR-005), not
# part of this framework. See safeAPIRBC2oo2's tests/posix_backend/.
