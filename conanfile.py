from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, rmdir
import os

class SpdlogConan(ConanFile):
    name = "cel_spdlog"  # Changed from "spdlog" to "cel_spdlog"
    version = "1.12.0"  # Update this to match the current spdlog version
    license = "MIT"
    url = "https://github.com/hncelestialtech/spdlog"
    description = "Fast C++ logging library"
    topics = ("logging", "header-only", "c++")
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "header_only": [True, False],
        "wchar_support": [True, False],
        "wchar_filenames": [True, False],
        "no_exceptions": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "header_only": False,
        "wchar_support": False,
        "wchar_filenames": False,
        "no_exceptions": False,
    }

    exports_sources = "CMakeLists.txt", "include/*", "src/*", "cmake/*"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.header_only:
            del self.options.shared
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["SPDLOG_BUILD_EXAMPLE"] = False
        tc.variables["SPDLOG_BUILD_EXAMPLE_HO"] = False
        tc.variables["SPDLOG_BUILD_TESTS"] = False
        tc.variables["SPDLOG_BUILD_TESTS_HO"] = False
        tc.variables["SPDLOG_BUILD_BENCH"] = False
        tc.variables["SPDLOG_FMT_EXTERNAL"] = False
        tc.variables["SPDLOG_BUILD_SHARED"] = self.options.get_safe("shared", False)
        tc.variables["SPDLOG_WCHAR_SUPPORT"] = self.options.wchar_support
        tc.variables["SPDLOG_WCHAR_FILENAMES"] = self.options.wchar_filenames
        tc.variables["SPDLOG_NO_EXCEPTIONS"] = self.options.no_exceptions
        tc.generate()

    def build(self):
        if not self.options.header_only:
            cmake = CMake(self)
            cmake.configure()
            cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*.h", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))
        if not self.options.header_only:
            copy(self, "*.lib", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
            copy(self, "*.a", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
            copy(self, "*.so*", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
            copy(self, "*.dylib", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
            copy(self, "*.dll", src=self.build_folder, dst=os.path.join(self.package_folder, "bin"), keep_path=False)
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        if self.options.header_only:
            self.cpp_info.bindirs = []
            self.cpp_info.libdirs = []
        else:
            self.cpp_info.libs = ["spdlog"]  # Note: The library name remains "spdlog"
        
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["pthread"]
        
        self.cpp_info.set_property("cmake_file_name", "cel_spdlog")  # Changed from "spdlog" to "cel_spdlog"
        self.cpp_info.set_property("cmake_target_name", "cel_spdlog::cel_spdlog")  # Changed from "spdlog::spdlog" to "cel_spdlog::cel_spdlog"
        
        # TODO: to remove in conan v2 once cmake_find_package_* generators removed
        self.cpp_info.names["cmake_find_package"] = "cel_spdlog"  # Changed from "spdlog" to "cel_spdlog"
        self.cpp_info.names["cmake_find_package_multi"] = "cel_spdlog"  # Changed from "spdlog" to "cel_spdlog"