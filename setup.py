import os
import pathlib
import platform
import sys
import shutil

import setuptools as st
import setuptools.command.build_ext as be


class CMakeExtension(st.Extension):
    def __init__(self, name):
        super().__init__(name, sources=[])


class build_ext(be.build_ext):
    def run(self):
        for ext in self.extensions:
            self.build_cmake(ext)

        super().run()

    def build_cmake(self, ext):
        cwd = pathlib.Path().absolute()

        build_temp = pathlib.Path(self.build_temp).absolute()
        shutil.rmtree(build_temp, ignore_errors=True)
        build_temp.mkdir(parents=True, exist_ok=True)

        extpath = pathlib.Path(self.get_ext_fullpath(ext.name)).absolute()
        extdir = extpath.parent
        shutil.rmtree(extpath, ignore_errors=True)
        extdir.mkdir(parents=True, exist_ok=True)

        # example of cmake args
        config = "Debug" if self.debug else "Release"
        arch = "x64" if platform.architecture()[0] == "64bit" else "x86"

        os.chdir(str(build_temp))
        self.spawn(
            [
                "cmake",
                str(cwd),
                "-DCMAKE_BUILD_TYPE=" + config,
                "-DPYTHON_EXECUTABLE=" + sys.executable,
                "-A",
                arch,
            ]
        )

        if not self.dry_run:
            self.spawn(
                [
                    "cmake",
                    "--build",
                    ".",
                    "--target",
                    ext.name,
                    "--config",
                    config,
                    "-j",
                    "4",
                ]
            )

            for f in pathlib.Path(".").glob("**/" + extpath.name):
                print("Copying {} to {}".format(f, extdir))
                shutil.copy(f, cwd / extdir)

        os.chdir(str(cwd))


st.setup(
    name="tinygeo",
    version="0.1",
    ext_modules=[CMakeExtension("tinygeo")],
    cmdclass={
        "build_ext": build_ext,
    },
)
