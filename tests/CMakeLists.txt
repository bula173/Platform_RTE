# The tests use assert() as their check, often around calls with side effects; -DNDEBUG (Release) would turn them into
# no-ops (unused variables and functions, and checks that silently never run).
add_compile_options(-UNDEBUG)

# Minimal assert()-based CTest harness placeholder.
# TODO: replace with Unity/CMock (or equivalent qualified framework) before
# this is used as SIL 3/4 verification evidence - see ADR-001 section 7.

function(rte_add_test name source lib)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ${lib})
    rte_set_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# ADR-023: each test links whichever of the 4 consolidated libraries
# (core/oal/channels/appmanager) covers the module under test - PUBLIC
# linking inside those libraries means one name still pulls in
# everything transitively required, exactly as the old fine-grained set
# of individual rte::<feature> targets did.
#
# ADR-024: each optional module's test is now gated behind the same
# RTE_ENABLE_* option that gates the module's own source file, so
# `cmake -DRTE_ENABLE_WATCHDOG=OFF` doesn't leave a dangling test that
# tries to link a library the build no longer produces.

# CORE - always built, no toggle.
rte_add_test(test_rte_status         status/test_rte_status.c            rte::core)
rte_add_test(test_rte_buffer         buffer/test_rte_buffer.c            rte::core)
rte_add_test(test_rte_cast           cast/test_rte_cast.c                rte::core)
rte_add_test(test_rte_safestate      safestate/test_rte_safestate.c      rte::core)
rte_add_test(test_rte_safety_violation safestate/test_rte_safety_violation.c rte::core)
rte_add_test(test_rte_string         string/test_rte_string.c            rte::core)
rte_add_test(test_rte_lifecycle      lifecycle/test_rte_lifecycle.c      rte::core)

# OAL
if(RTE_ENABLE_TIMER)
    rte_add_test(test_rte_timer      timer/test_rte_timer.c              rte::oal)
endif()
if(RTE_ENABLE_NVM)
    rte_add_test(test_rte_nvm        nvm/test_rte_nvm.c                  rte::oal)
endif()
if(RTE_ENABLE_MEMORY)
    rte_add_test(test_rte_memory     memory/test_rte_memory.c            rte::oal)
    rte_add_test(test_rte_safe_ptr   memory/test_rte_safe_ptr.c          rte::oal)
endif()
if(RTE_ENABLE_TASK)
    rte_add_test(test_rte_task       task/test_rte_task.c                rte::oal)
endif()
if(RTE_ENABLE_IPC)
    rte_add_test(test_rte_ipc        ipc/test_rte_ipc.c                  rte::oal)
endif()
if(RTE_ENABLE_REBOOT)
    rte_add_test(test_rte_reboot     reboot/test_rte_reboot.c            rte::oal)
endif()
if(RTE_ENABLE_PLATFORM)
    rte_add_test(test_rte_platform   platform/test_rte_platform.c        rte::oal)
endif()
if(RTE_ENABLE_NETLINK)
    rte_add_test(test_rte_netlink    netlink/test_rte_netlink.c          rte::oal)
endif()
if(RTE_ENABLE_FLOW)
    rte_add_test(test_rte_flow       flow/test_rte_flow.c                rte::oal)
endif()
if(RTE_ENABLE_LOG)
    rte_add_test(test_rte_log        log/test_rte_log.c                  rte::oal)
endif()
if(RTE_ENABLE_WATCHDOG)
    rte_add_test(test_rte_watchdog   watchdog/test_rte_watchdog.c        rte::oal)
endif()

rte_add_test(test_rte_osadapter      osadapter/test_rte_osadapter.c      rte::channels)

# Channels
if(RTE_ENABLE_CHECKSUM)
    rte_add_test(test_rte_checksum   checksum/test_rte_checksum.c        rte::channels)
endif()
if(RTE_ENABLE_CHANNEL_LINK)
    rte_add_test(test_rte_channel channel_link/test_rte_channel.c rte::channels)
endif()
if(RTE_ENABLE_CHANNEL_LINK AND RTE_ENABLE_FLOW)
    rte_add_test(test_rte_channel_service_flow_osadapter channel_service/test_rte_channel_service_flow_osadapter.c rte::channels)
endif()
if(RTE_ENABLE_VOTER)
    rte_add_test(test_rte_voter       voter/test_rte_voter.c              rte::channels)
endif()
if(RTE_ENABLE_VOTER AND RTE_ENABLE_REDUNDANCY_CONFIG)
    rte_add_test(test_rte_redundancy_config redundancy_config/test_rte_redundancy_config.c rte::channels)
endif()
if(RTE_ENABLE_STATE_TRANSFER)
    rte_add_test(test_rte_state_transfer state_transfer/test_rte_state_transfer.c rte::channels)
endif()
if(RTE_ENABLE_CROSS_COMPARATOR)
    rte_add_test(test_rte_cross_comparator cross_comparator/test_rte_cross_comparator.c rte::channels)
endif()
if(RTE_ENABLE_CLOCKSYNC)
    rte_add_test(test_rte_clocksync  clocksync/test_rte_clocksync.c      rte::channels)
endif()
if(RTE_ENABLE_CHECKPOINT)
    rte_add_test(test_rte_checkpoint checkpoint/test_rte_checkpoint.c    rte::channels)
endif()
if(RTE_ENABLE_DUAL)
    rte_add_test(test_rte_dual_types      dual/test_rte_dual_types.c      rte::channels)
    rte_add_test(test_rte_dual_msgchannel dual/test_rte_dual_msgchannel.c rte::channels)
    rte_add_test(test_rte_dual_channel    dual/test_rte_dual_channel.c    rte::channels)
    rte_add_test(test_rte_dual_negotiator dual/test_rte_dual_negotiator.c rte::channels)
endif()
if(RTE_ENABLE_SAFECHANNEL)
    rte_add_test(test_rte_safechannel     safechannel/test_rte_safechannel.c rte::channels)
endif()

# App layer
if(RTE_ENABLE_APPMANAGER)
    rte_add_test(test_rte_appmanager appmanager/test_rte_appmanager.c    rte::appmanager)
endif()

# posix_osadapter (and its tests) moved to the safeAPIRBC2oo2 project - an
# OAL OSAdapter is an integrator-supplied implementation (ADR-005), not
# part of this framework. See safeAPIRBC2oo2's tests/posix_osadapter/.
