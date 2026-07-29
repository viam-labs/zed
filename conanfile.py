import os
import tarfile
import re
from tempfile import TemporaryDirectory

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, load


class ViamZed(ConanFile):
    name = "viam-camera-zed"
    license = "Apache-2.0"
    url = "https://github.com/viam-labs/zed"
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"
    default_options = {
        "viam-cpp-sdk/*:shared": False,
    }

    exports_sources = "CMakeLists.txt", "LICENSE", "src/*", "meta.json", "*.sh"

    version = "0.0.1"

    def set_version(self):
        content = load(self, "CMakeLists.txt")
        self.version = re.search(r"set\(CMAKE_PROJECT_VERSION (.+)\)", content).group(1).strip()

    def validate(self):
        check_min_cppstd(self, 17)

    def requirements(self):
        self.requires("viam-cpp-sdk/0.39.0")
        # Pin transitive deps to versions with prebuilt binaries (avoid slow source builds).
        self.requires("grpc/1.72.0", override=True)
        self.requires("protobuf/5.27.0", override=True)
        self.requires("abseil/20250127.0", override=True)
        self.requires("re2/20230301", override=True)
        self.requires("openssl/3.6.0", override=True)
        self.requires("zlib/1.3.1", override=True)
        self.requires("xtensor/[>=0.24.3 <0.27.0]", override=True)

    def layout(self):
        cmake_layout(self, src_folder=".")

    def generate(self):
        CMakeToolchain(self).generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()

    def deploy(self):
        with TemporaryDirectory(dir=self.deploy_folder) as tmp_dir:
            self.output.info("Deploying necessary files to module.tar.gz")

            copy(self, "viam-camera-zed", src=self.package_folder, dst=tmp_dir)
            copy(self, "meta.json", src=self.package_folder, dst=tmp_dir)
            copy(self, "*.sh", src=self.package_folder, dst=tmp_dir)

            self.output.info("Creating module.tar.gz")
            with tarfile.open(os.path.join(self.deploy_folder, "module.tar.gz"), "w|gz") as tar:
                tar.add(tmp_dir, arcname=".", recursive=True)
                for mem in tar.getmembers():
                    self.output.info(mem.name)
