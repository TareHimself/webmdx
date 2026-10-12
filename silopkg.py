from typing import NamedTuple

from silo import Component, InvalidConfiguration, Options, Package, SiloPackage
from silo.tools.cmake import CMakeBuild, CMakeDeps, CMakeGenerator, CMakePresets


class Codec(NamedTuple):
    package: str
    version: str
    # The target CMakeLists.txt links, when it is not the one silo names
    cmake_target: str | None = None


# Keyed by the option that turns each one on
VIDEO_CODECS = {
    "with_vpx": Codec("libvpx", "1.14.1"),
    "with_av1": Codec("dav1d", "1.4.3"),
}
AUDIO_CODECS = {
    "with_opus": Codec("opus", "1.5.2", "Opus::opus"),
    "with_vorbis": Codec("vorbis", "1.3.7", "Vorbis::vorbis"),
}
CODECS = VIDEO_CODECS | AUDIO_CODECS


class WebmdxOptions(Options):
    shared: bool = True
    # VP8 and VP9 video, through libvpx
    with_vpx: bool = True
    # AV1 video, through dav1d
    with_av1: bool = True
    # Opus audio, through libopus
    with_opus: bool = True
    # Vorbis audio, through libvorbis
    with_vorbis: bool = True


class Webmdx(SiloPackage[WebmdxOptions]):
    name = "webmdx"
    version = "1.0.0"
    # The public headers use std::span
    min_cppstd = 20
    supports_shared = True

    def codecs(self):
        return [codec for option, codec in CODECS.items() if getattr(self.options, option)]

    def source(self, src):
        src.local(".")

    def deps(self):
        self.dep("libwebm@1.0.0.31")
        self.dep("libyuv@1974")
        for codec in self.codecs():
            self.dep(f"{codec.package}@{codec.version}")
        self.tool("cmake@4.3")
        self.tool("ninja@1.13")

    def validate(self):
        for kind, codecs in (("video", VIDEO_CODECS), ("audio", AUDIO_CODECS)):
            if not any(getattr(self.options, option) for option in codecs):
                names = " or ".join(codecs)
                raise InvalidConfiguration(f"webmdx needs one {kind} codec: {names}")

    def generate(self):
        deps = CMakeDeps(self)
        deps.alias("webm::webm", "libwebm::webm")
        for codec in self.codecs():
            if codec.cmake_target:
                deps.alias(codec.cmake_target, f"{codec.package}::{codec.package}")
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
            library.requires += ["libwebm::webm", "libyuv"]
            library.requires += [codec.package for codec in self.codecs()]
        return Package(components=[library], licenses=["LICENSE"])
