import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy


class SafeAPIFrameworkConan(ConanFile):
    name = "safeapiframework"
    version = "0.1.0"
    license = "see LICENSE.md"
    description = (
        "Layered C99 API framework for safety-related railway applications "
        "(ERTMS Radio Block Centre, CENELEC EN 50128 / SIL 4): timers, IPC, "
        "memory, NVM, task/thread, logging, reboot, watchdog, app manager, "
        "safe-state transitions, and a redundancy framework (channels/voting, "
        "checksums, checkpoints, clock sync)."
    )
    url = "https://github.com/bula173/safeAPIFreamwork"
    package_type = "static-library"
    settings = "os", "compiler", "build_type", "arch"

    # Mirrors CMakeLists.txt's own SAFEAPI_ENABLE_* options (ADR-024
    # feature-selectable build) one-for-one, so a Conan-driven build has the
    # same granularity as a plain `cmake -DSAFEAPI_ENABLE_...=OFF` one.
    options = {
        "fPIC": [True, False],
        "with_timer": [True, False],
        "with_nvm": [True, False],
        "with_memory": [True, False],
        "with_task": [True, False],
        "with_netlink": [True, False],
        "with_ipc": [True, False],
        "with_reboot": [True, False],
        "with_log": [True, False],
        "with_watchdog": [True, False],
        "with_clocksync": [True, False],
        "with_checksum": [True, False],
        "with_channel_link": [True, False],
        "with_voter": [True, False],
        "with_cross_comparator": [True, False],
        "with_checkpoint": [True, False],
        "with_dual": [True, False],
        "with_safechannel": [True, False],
        "with_appmanager": [True, False],
    }
    default_options = {name: True for name in options} | {"fPIC": True}

    # SAFEAPI_ENABLE_<X> CMake option name for each with_<x> Conan option
    # above (explicit list, not introspected from self.options - Conan 2's
    # Options object has no public "list the defined option names" API).
    _feature_options = [
        "with_timer", "with_nvm", "with_memory", "with_task", "with_netlink",
        "with_ipc", "with_reboot", "with_log", "with_watchdog", "with_clocksync",
        "with_checksum", "with_channel_link", "with_voter", "with_cross_comparator",
        "with_checkpoint", "with_dual", "with_safechannel", "with_appmanager",
    ]

    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "include/*",
        "src/*",
        "tools/*",
        "LICENSE.md",
    )

    def _dist_platform_dir(self):
        # Mirrors SAFEAPI_DIST_PLATFORM_DIR (CMAKE_SYSTEM_NAME-CMAKE_SYSTEM_PROCESSOR,
        # see CMakeLists.txt and root CLAUDE.md's "dist/" convention) - Conan's
        # own os/arch settings use different vocabulary (Macos/armv8 vs CMake's
        # Darwin/arm64), and CMAKE_SYSTEM_PROCESSOR for the same CPU family
        # differs BY OS too (arm64 on Darwin, aarch64 on Linux) - this table
        # covers exactly the platforms this workspace's own toolchain projects
        # enumerate (LinuxMacOSToolchain's Toolchain-Linux.cmake, SAPIMacOSToolchain).
        # Duplicated per-project rather than shared, same convention
        # compute_version.sh already uses (every project here is independently
        # clonable).
        os_map = {"Macos": "Darwin", "Linux": "Linux"}
        arch_map = {
            "Darwin": {"armv8": "arm64", "x86_64": "x86_64"},
            "Linux": {"armv8": "aarch64", "armv7": "arm", "x86_64": "x86_64", "x86": "i686"},
        }
        cmake_os = os_map.get(str(self.settings.os), str(self.settings.os))
        cmake_arch = arch_map.get(cmake_os, {}).get(str(self.settings.arch), str(self.settings.arch))
        return f"{cmake_os}-{cmake_arch}"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["SAFEAPI_BUILD_TESTS"] = False
        if self.options.get_safe("fPIC") is not None:
            tc.cache_variables["CMAKE_POSITION_INDEPENDENT_CODE"] = bool(self.options.fPIC)
        for opt_name in self._feature_options:
            cmake_var = "SAFEAPI_ENABLE_" + opt_name[len("with_"):].upper()
            tc.cache_variables[cmake_var] = bool(getattr(self.options, opt_name))
        tc.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.md", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        # This package installs to <package_folder>/dist/... (see root
        # CLAUDE.md's "dist/" convention and this project's own
        # CMakeLists.txt "Package and export configuration" block) - not the
        # Conan-conventional <package_folder>/include, /lib directly, so
        # every path below is dist-prefixed. Component graph mirrors
        # target_link_libraries() in CMakeLists.txt / src/appmanager/CMakeLists.txt
        # exactly, and cmake_target_name matches the safeapi::* names our own
        # install(EXPORT ...) already exports - a consumer's
        # target_link_libraries(x PRIVATE safeapi::core) works identically
        # whether resolved via Conan's CMakeDeps or via this project's own
        # dist/lib/cmake/safeAPIFramework/safeAPIFrameworkConfig.cmake.
        self.cpp_info.set_property("cmake_file_name", "safeAPIFramework")

        plat = self._dist_platform_dir()
        includedir = f"dist/{plat}/include"
        libdir = f"dist/{plat}/lib"

        self.cpp_info.components["core"].set_property("cmake_target_name", "safeapi::core")
        self.cpp_info.components["core"].libs = ["safeapi_core"]
        self.cpp_info.components["core"].includedirs = [includedir]
        self.cpp_info.components["core"].libdirs = [libdir]

        self.cpp_info.components["oal"].set_property("cmake_target_name", "safeapi::oal")
        self.cpp_info.components["oal"].libs = ["safeapi_oal"]
        self.cpp_info.components["oal"].includedirs = [includedir]
        self.cpp_info.components["oal"].libdirs = [libdir]
        self.cpp_info.components["oal"].requires = ["core"]

        self.cpp_info.components["channels"].set_property("cmake_target_name", "safeapi::channels")
        self.cpp_info.components["channels"].libs = ["safeapi_channels"]
        self.cpp_info.components["channels"].includedirs = [includedir]
        self.cpp_info.components["channels"].libdirs = [libdir]
        self.cpp_info.components["channels"].requires = ["core", "oal"]

        if self.options.with_appmanager:
            self.cpp_info.components["appmanager"].set_property("cmake_target_name", "safeapi::appmanager")
            self.cpp_info.components["appmanager"].libs = ["safeapi_appmanager"]
            self.cpp_info.components["appmanager"].includedirs = [includedir]
            self.cpp_info.components["appmanager"].libdirs = [libdir]
            self.cpp_info.components["appmanager"].requires = ["core", "oal", "channels"]
