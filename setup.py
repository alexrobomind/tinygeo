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

        # these dirs will be created in build_py, so if you don't have
        # any python sources to bundle, the dirs will be missing
        build_temp = pathlib.Path(self.build_temp)
        build_temp.mkdir(parents=True, exist_ok=True)
        
        #build_root = build_temp / "build"
        #build_root.mkdir(parents=True, exist_ok=True)
        
        #install_root = build_temp / "install"
        #install_root.mkdir(parents=True, exist_ok=True)
        
        extdir = pathlib.Path(self.get_ext_fullpath(ext.name)).parent
        extdir.mkdir(parents=True, exist_ok=True)
        
        print('BT is {}'.format(build_temp))
        print('ED is {}'.format(extdir))

        # example of cmake args
        config = "Debug" if self.debug else "Release"
        arch = "x64" if platform.architecture()[0] == "64bit" else "x86"
        cmake_args = [
            "-DCMAKE_BUILD_TYPE=" + config,
            "-DPYTHON_EXECUTABLE=" + sys.executable,
            "-A",
            arch,
        ]
        
        if platform.system() == "Windows":
            pass
        else:
            cmake_args += ['-DCMAKE_BUILD_TYPE=' + config]

        # example of build args
        build_args = ["--config", config, "-j", "4"]

        os.chdir(str(build_temp))
        self.spawn(["cmake", str(cwd)] + cmake_args)
        if not self.dry_run:
            self.spawn(["cmake", "--build", ".", "--target", "tinyrgeo"] + build_args)
            
            for file_ext in [".so", ".pyd"]:
                for f in pathlib.Path(".").glob("**/tinyrgeo*" + file_ext):
                    print("Copying {} to {}".format(f, extdir))
                    shutil.copy(f, cwd / extdir)
        # Troubleshooting: if fail on line above then delete all possible
        # temporary CMake files including "CMakeCache.txt" in top level dir.
        os.chdir(str(cwd))


st.setup(
    name="spam",
    version="0.1",
    ext_modules=[CMakeExtension("tinyrgeo")],
    cmdclass={
        "build_ext": build_ext,
    },
)
