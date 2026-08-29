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
        tc.generate()

    def requirements(self):
        # oiter dependencies
        self.requires("imgui/1.92.8")
        self.requires("lyra/1.7.0")

        # 2iREN dependencies
        # Conan will only evaluate the root recipe, and since we have 2iREN
        # as a git submodule, we need to copy 2iREN's dependencies here
        self.requires("yaml-cpp/0.9.0")
        self.requires("glm/1.0.1")
        self.requires("opengl/system")
        self.requires("glfw/3.4", options={"with_wayland": False})
        self.requires(
            "glad/2.0.8",
            options={
                "gl_version": "4.6",
                "gl_profile": "core",
            },
        )
