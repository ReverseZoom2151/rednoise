from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy
import os


class RednoiseConan(ConanFile):
    name = "rednoise"
    version = "0.1.0"
    license = "MIT"
    author = "ReverseZoom2151"
    url = "https://github.com/ReverseZoom2151/rednoise"
    homepage = "https://github.com/ReverseZoom2151/rednoise"
    description = (
        "RedNoise: a CPU software renderer (wireframe, rasteriser, ray tracer, "
        "path tracer) with a stable C ABI."
    )
    topics = ("renderer", "ray-tracing", "path-tracing", "graphics", "c-abi")

    settings = "os", "compiler", "build_type", "arch"
    package_type = "library"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    # This recipe builds RedNoise from the repository sources. It is meant to be
    # run from the repo root, e.g. `conan create packaging/conan`, and exports the
    # whole tree (CMakeLists.txt, src/, framework/, include/, cmake/, examples/,
    # LICENSE) that the CMake build needs. `..` reaches the repo root relative to
    # this recipe living in packaging/conan.
    exports_sources = (
        "../../CMakeLists.txt",
        "../../src/*",
        "../../framework/*",
        "../../include/*",
        "../../cmake/*",
        "../../examples/*",
        "../../tools/*",
        "../../third_party/*",
        "../../LICENSE",
    )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("glm/1.0.1")

    def layout(self):
        # The exported sources place the repo root (with CMakeLists.txt) at the
        # export folder root, so point the CMake layout's src there.
        cmake_layout(self, src_folder=".")

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_APP"] = "OFF"
        tc.cache_variables["BUILD_GPU"] = "OFF"
        tc.cache_variables["BUILD_HEADLESS"] = "OFF"
        tc.cache_variables["BUILD_TESTS"] = "OFF"
        tc.cache_variables["BUILD_LIB"] = "ON"
        # Use the glm provided by Conan instead of fetching from upstream.
        tc.cache_variables["FETCH_DEPENDENCIES"] = "OFF"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(
            self,
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "rednoise")
        self.cpp_info.set_property("cmake_target_name", "rednoise::rednoise")
        self.cpp_info.libs = ["rednoise"]
