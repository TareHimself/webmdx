from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
import os 

class Webmdx(ConanFile):
    name = "webmdx"
    version = "1.0.0"
    url = "https://github.com/TareHimself/webmdx.git"
    license = "MIT"
    requires = [
        "dav1d/1.4.3",
        "libvpx/1.14.1",
        "libwebm/1.0.0.31",
        "libyuv/1892",
        "opus/1.5.2",
    ]
    git_tag = "main"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = "CMakeLists.txt", "lib/*", "include/*"
    options = {
            "shared": [True, False],
        }
    default_options = {
        "shared": True,
    }
    
    def config_options(self):
        pass

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        #tc.variables["CMAKE_CXX_STANDARD"] = "20"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "webmdx")
        self.cpp_info.set_property("cmake_target_name", "webmdx::webmdx")
        self.cpp_info.set_property("pkg_config_name", "webmdx")
        self.cpp_info.libs = ["webmdx"]
            
