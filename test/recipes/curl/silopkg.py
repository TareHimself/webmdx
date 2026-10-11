from silo import Component, InvalidConfiguration, Package, SiloPackage
from silo.enums import Platform
from silo.tools.conan import ConanInstall

# cpr is built against a libcurl with these options and has to be told the same ones
CONAN_OPTIONS = {"with_ssl": "schannel", "with_zlib": False}


class Curl(SiloPackage):
    name = "curl"
    version = "8.22.0"
    languages = ("c",)

    def source(self, src):
        src.empty()

    def deps(self):
        self.tool("conan@2")

    def validate(self):
        # Only the Windows TLS backend needs no dependency of its own
        if self.profile.settings.os is not Platform.Windows:
            raise InvalidConfiguration("curl is only packaged for Windows so far")

    def build(self):
        ConanInstall(self).install(
            "libcurl/8.22.0#3599f8564660d38f96af52da40795734", options=CONAN_OPTIONS
        )

    def package(self):
        component = Component(
            "libcurl",
            headers=["include"],
            static_libs=["libcurl"],
            defines=["CURL_STATICLIB=1"],
            system_libs=["ws2_32", "bcrypt", "iphlpapi", "crypt32", "secur32"],
            content=["licenses"],
        )
        return Package(components=[component])
