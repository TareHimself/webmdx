from silo import Component, Package, SiloPackage
from silo.tools.conan import ConanInstall


class Cpr(SiloPackage):
    name = "cpr"
    version = "1.14.2"

    def source(self, src):
        src.empty()

    def deps(self):
        self.dep("../curl")
        self.tool("conan@2")
        # Conan Center has no cpr binary for MSVC, and its recipe builds with the cmake it finds
        self.tool("cmake@4.3")

    def build(self):
        # Conan builds cpr against its own libcurl, which has to match the curl recipe's
        ConanInstall(self).install(
            "cpr/1.14.2#6465cfddec5dc924e1a6ba8204600b80",
            options={
                "with_ssl": "winssl",
                "libcurl/*:with_ssl": "schannel",
                "libcurl/*:with_zlib": False,
            },
        )

    def package(self):
        component = Component(
            "cpr",
            headers=["include"],
            static_libs=["cpr"],
            requires=["curl::libcurl"],
            content=["licenses"],
        )
        return Package(components=[component])
