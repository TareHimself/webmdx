from silo import Component, Options, Package, SiloPackage
from silo.tools.cmake import CMakeBuild, CMakeDeps, CMakeGenerator, CMakePresets

CODECS = {"libvpx": "1.14.1", "dav1d": "1.4.3", "opus": "1.5.2"}


class WebmdxOptions(Options):
    shared: bool = True


class Webmdx(SiloPackage[WebmdxOptions]):
    name = "webmdx"
    version = "1.0.0"
    supports_shared = True

    def source(self, src):
        src.local(".")

    def deps(self):
        self.dep("libwebm@1.0.0.31")
        self.dep("libyuv@1974")
        for codec, version in CODECS.items():
            self.dep(f"{codec}@{version}")
        self.tool("cmake@4.3")
        self.tool("ninja@1.13")

    def generate(self):
        deps = CMakeDeps(self)
        deps.alias("webm::webm", "libwebm::webm")
        deps.alias("Opus::opus", "opus::opus")
        deps.generate()
        toolchain = CMakeGenerator(self)
        toolchain.add_cache_bool("BUILD_SHARED_LIBS", self.options.shared)
        toolchain.generate()

        # After `silo sync`: cmake --preset webmdx-debug && cmake --build --preset webmdx-debug
        name = "debug" if self.is_debug else "release"
        presets = CMakePresets(self, toolchain)
        presets.add(f"webmdx-{name}", build_dir=f"build/{name}")
        presets.generate()

    def build(self):
        cmake = CMakeBuild(self)
        cmake.configure()
        cmake.build()
        cmake.install()

    def package(self):
        library = Component("webmdx", headers=["include"])
        if self.options.shared:
            library.shared_libs.append("webmdx")
        else:
            library.static_libs.append("webmdx")
            library.defines.append("WEBMDX_STATIC")
            library.requires += ["libwebm::webm", "libyuv", *CODECS]
        return Package(components=[library], licenses=["LICENSE"])
