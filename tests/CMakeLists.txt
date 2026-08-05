# Minimal assert()-based CTest harness placeholder.
# TODO: replace with Unity/CMock (or equivalent qualified framework) before
# this is used as SIL 3/4 verification evidence - see ADR-001 section 7.

function(safeapi_add_test name source lib)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ${lib})
    safeapi_set_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

safeapi_add_test(test_sapi_status         status/test_sapi_status.c            safeapi::status)
safeapi_add_test(test_sapi_buffer         buffer/test_sapi_buffer.c            safeapi::buffer)
safeapi_add_test(test_sapi_cast           cast/test_sapi_cast.c                safeapi::cast)
safeapi_add_test(test_sapi_safestate      safestate/test_sapi_safestate.c      safeapi::safestate)
safeapi_add_test(test_sapi_string         string/test_sapi_string.c            safeapi::string)
safeapi_add_test(test_sapi_timer          timer/test_sapi_timer.c              safeapi::timer)
safeapi_add_test(test_sapi_nvm            nvm/test_sapi_nvm.c                  safeapi::nvm)
safeapi_add_test(test_sapi_reboot         reboot/test_sapi_reboot.c            safeapi::reboot)
safeapi_add_test(test_sapi_vital_channel  vital_channel/test_sapi_vital_channel.c safeapi::vital_channel)
safeapi_add_test(test_sapi_clocksync      clocksync/test_sapi_clocksync.c      safeapi::clocksync)
safeapi_add_test(test_sapi_checkpoint     checkpoint/test_sapi_checkpoint.c    safeapi::checkpoint)
safeapi_add_test(test_sapi_netlink        netlink/test_sapi_netlink.c          safeapi::netlink)

# posix_backend (and its tests) moved to the safeAPIExample project - an
# OAL backend is an integrator-supplied implementation (ADR-005), not
# part of this framework. See safeAPIExample's tests/posix_backend/.
