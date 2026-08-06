from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain, CMakeDeps


class Oiter(ConanFile):
    name = "oiter"
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        CMakeDeps(self).generate()

        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        tc.generate()

    def requirements(self):
        requirements = self.conan_data.get('requirements', [])

        for requirement in requirements:
            if isinstance(requirement, dict):
                self.requires(
                    requirement["ref"],
                    options=requirement.get("options", {})
                )
            else:
                self.requires(requirement)
