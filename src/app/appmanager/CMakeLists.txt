add_library(rte_appmanager ${RTE_LIB_TYPE} rte_appmanager.c)

target_include_directories(rte_appmanager
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link against the 3 consolidated safeAPI libraries (ADR-023) appmanager
# depends on: rte_core (status), rte_oal (log), rte_channels
# (checkpoint - required by the optional ADR-019 checkpoint integration,
# rte_channel_checkpoint() is called from rte_appmanager_run() whenever
# config->checkpoint is non-NULL).
target_link_libraries(rte_appmanager PUBLIC rte_core rte_oal rte_channels)

rte_set_warnings(rte_appmanager)
set_target_properties(rte_appmanager PROPERTIES EXPORT_NAME appmanager)

add_library(rte::appmanager ALIAS rte_appmanager)
