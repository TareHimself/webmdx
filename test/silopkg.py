from silo import SiloPackage
from silo.tools.cmake import CMakeBuild, CMakeDeps, CMakeGenerator, CMakePresets


class WebmdxTest(SiloPackage):
    name = "webmdx-test"
    version = "1.0.0"

    def source(self, src):
        src.local(".")

    def deps(self):
        self.dep("..")
        self.dep("recipes/cpr")
        self.tool("cmake@4.3")
        self.tool("ninja@1.13")

    def generate(self):
        CMakeDeps(self).generate()
        toolchain = CMakeGenerator(self)
        toolchain.generate()

        # After `silo sync`: cmake --preset webmdx-test-debug && cmake --build --preset webmdx-test-debug
        name = "debug" if self.is_debug else "release"
        presets = CMakePresets(self, toolchain)
        presets.add(f"webmdx-test-{name}", build_dir=f"build/{name}")
        presets.generate()

    def build(self):
        cmake = CMakeBuild(self)
        cmake.configure()
        cmake.build()
        cmake.install()
