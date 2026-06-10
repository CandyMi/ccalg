"""Conan 2.x recipe for ccalg — header-only data-structure library.

Usage:
    # Install locally
    conan create . -tf ""

    # Consume in your project's conanfile.txt:
    # [requires]
    # ccalg/0.1.0

    # Or conanfile.py:
    # self.requires("ccalg/0.1.0")
"""

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy
from conan.tools.build import check_min_cppstd
import os


class CcalgConan(ConanFile):
    name = "ccalg"
    version = "0.1.0"
    license = "BSD-3-Clause"
    author = "CandyMi"
    url = "https://github.com/CandyMi/ccalg"
    homepage = "https://ccalg.dev"
    description = (
        "A header-only library for high-performance data structures in C/C++. "
        "Provides intrusive red-black tree (ccmap), hashmap (cchashmap), "
        "doubly/singly linked lists (cclist/cclink), d-ary heap (ccheap), "
        "dynamic array (ccvector), sorted-array map (ccflatmap), "
        "and treap with order statistics (cctreap). "
        "Compatible with C89/C99/C++/MSVC. Zero internal node allocation."
    )
    topics = ("data-structures", "header-only", "intrusive", "c", "c-plus-plus")
    package_type = "header-library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }
    generators = "CMakeDeps"

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def layout(self):
        cmake_layout(self)

    def package_id(self):
        self.info.clear()

    def validate(self):
        pass

    def source(self):
        copy(self, "CMakeLists.txt", src=self.source_folder, dst=self.source_folder)
        copy(self, "include/*", src=self.source_folder, dst=self.source_folder)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.install()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

        # Headers are in include/ccalg/*.h, users include as <ccalg/ccmap.h>
        self.cpp_info.includedirs = ["include"]

        # Each header is self-contained; no link dependencies
        self.cpp_info.set_property("cmake_file_name", "ccalg")
        self.cpp_info.set_property("cmake_target_name", "ccalg::ccalg")
