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
safeapi_add_test(test_rte_status         status/test_rte_status.c            safeapi::core)
safeapi_add_test(test_rte_buffer         buffer/test_rte_buffer.c            safeapi::core)
safeapi_add_test(test_rte_cast           cast/test_rte_cast.c                safeapi::core)
safeapi_add_test(test_rte_safestate      safestate/test_rte_safestate.c      safeapi::core)
safeapi_add_test(test_rte_safety_violation safestate/test_rte_safety_violation.c safeapi::core)
safeapi_add_test(test_rte_string         string/test_rte_string.c            safeapi::core)
safeapi_add_test(test_rte_lifecycle      lifecycle/test_rte_lifecycle.c      safeapi::core)

# OAL
if(SAFEAPI_ENABLE_TIMER)
    safeapi_add_test(test_rte_timer      timer/test_rte_timer.c              safeapi::oal)
endif()
if(SAFEAPI_ENABLE_NVM)
    safeapi_add_test(test_rte_nvm        nvm/test_rte_nvm.c                  safeapi::oal)
endif()
if(SAFEAPI_ENABLE_MEMORY)
    safeapi_add_test(test_rte_memory     memory/test_rte_memory.c            safeapi::oal)
    safeapi_add_test(test_rte_safe_ptr   memory/test_rte_safe_ptr.c          safeapi::oal)
endif()
if(SAFEAPI_ENABLE_TASK)
    safeapi_add_test(test_rte_task       task/test_rte_task.c                safeapi::oal)
endif()
if(SAFEAPI_ENABLE_IPC)
    safeapi_add_test(test_rte_ipc        ipc/test_rte_ipc.c                  safeapi::oal)
endif()
if(SAFEAPI_ENABLE_REBOOT)
    safeapi_add_test(test_rte_reboot     reboot/test_rte_reboot.c            safeapi::oal)
endif()
if(SAFEAPI_ENABLE_PLATFORM)
    safeapi_add_test(test_rte_platform   platform/test_rte_platform.c        safeapi::oal)
endif()
if(SAFEAPI_ENABLE_NETLINK)
    safeapi_add_test(test_rte_netlink    netlink/test_rte_netlink.c          safeapi::oal)
endif()
if(SAFEAPI_ENABLE_FLOW)
    safeapi_add_test(test_rte_flow       flow/test_rte_flow.c                safeapi::oal)
endif()
if(SAFEAPI_ENABLE_LOG)
    safeapi_add_test(test_rte_log        log/test_rte_log.c                  safeapi::oal)
endif()
if(SAFEAPI_ENABLE_WATCHDOG)
    safeapi_add_test(test_rte_watchdog   watchdog/test_rte_watchdog.c        safeapi::oal)
endif()

safeapi_add_test(test_rte_osadapter      osadapter/test_rte_osadapter.c      safeapi::channels)

# Channels
if(SAFEAPI_ENABLE_CHECKSUM)
    safeapi_add_test(test_rte_checksum   checksum/test_rte_checksum.c        safeapi::channels)
endif()
if(SAFEAPI_ENABLE_CHANNEL_LINK)
    safeapi_add_test(test_rte_channel channel_link/test_rte_channel.c safeapi::channels)
endif()
if(SAFEAPI_ENABLE_CHANNEL_LINK AND SAFEAPI_ENABLE_FLOW)
    safeapi_add_test(test_rte_channel_service_flow_backend channel_service/test_rte_channel_service_flow_backend.c safeapi::channels)
endif()
if(SAFEAPI_ENABLE_VOTER)
    safeapi_add_test(test_rte_voter       voter/test_rte_voter.c              safeapi::channels)
endif()
if(SAFEAPI_ENABLE_VOTER AND SAFEAPI_ENABLE_REDUNDANCY_CONFIG)
    safeapi_add_test(test_rte_redundancy_config redundancy_config/test_rte_redundancy_config.c safeapi::channels)
endif()
if(SAFEAPI_ENABLE_STATE_TRANSFER)
    safeapi_add_test(test_rte_state_transfer state_transfer/test_rte_state_transfer.c safeapi::channels)
endif()
if(SAFEAPI_ENABLE_CROSS_COMPARATOR)
    safeapi_add_test(test_rte_cross_comparator cross_comparator/test_rte_cross_comparator.c safeapi::channels)
endif()
if(SAFEAPI_ENABLE_CLOCKSYNC)
    safeapi_add_test(test_rte_clocksync  clocksync/test_rte_clocksync.c      safeapi::channels)
endif()
if(SAFEAPI_ENABLE_CHECKPOINT)
    safeapi_add_test(test_rte_checkpoint checkpoint/test_rte_checkpoint.c    safeapi::channels)
endif()
if(SAFEAPI_ENABLE_DUAL)
    safeapi_add_test(test_rte_dual_types      dual/test_rte_dual_types.c      safeapi::channels)
    safeapi_add_test(test_rte_dual_msgchannel dual/test_rte_dual_msgchannel.c safeapi::channels)
    safeapi_add_test(test_rte_dual_channel    dual/test_rte_dual_channel.c    safeapi::channels)
    safeapi_add_test(test_rte_dual_negotiator dual/test_rte_dual_negotiator.c safeapi::channels)
endif()
if(SAFEAPI_ENABLE_SAFECHANNEL)
    safeapi_add_test(test_rte_safechannel     safechannel/test_rte_safechannel.c safeapi::channels)
endif()

# App layer
if(SAFEAPI_ENABLE_APPMANAGER)
    safeapi_add_test(test_rte_appmanager appmanager/test_rte_appmanager.c    safeapi::appmanager)
endif()

# posix_backend (and its tests) moved to the safeAPIRBC2oo2 project - an
# OAL backend is an integrator-supplied implementation (ADR-005), not
# part of this framework. See safeAPIRBC2oo2's tests/posix_backend/.
