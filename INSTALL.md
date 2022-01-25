# Build on Linux

Install build dependencies:

- git
- cmake
- g++
- NB(Mr.sun NB library is very **NB**) [NB](https://github.com/scarsty/nb)
- [SDL2](https://github.com/libsdl-org/SDL)

- OpenGL
- [yaml-cpp](https://github.com/jbeder/yaml-cpp)
(`sudo apt install libyaml-cpp-dev` or git-clone-and-compile-install)

**Git clone the nn-editor repo with submodule**

```bash
git clone https://github.com/scarsty/nn-editor
cd nn-editor
git submodule update --init 
```

**And you can use these to build this repo**

Please modify the **CMakeLists.txt**

About the IncludePath and link please make sure the yaml-cpp 

```bash
mkdir build
cd build
cmake ..
make
```

Please use apt (or yum or pacman or brew) to install SDL2 and yaml-cpp if you can.

**On MacOS**

```bash
mkdir build
cd build
cmake ..
make
```
or
```bash
cmake -G "Xcode"
```

**SDL2 install**

- The all-command-line method:
```bash
cd ~/work
git clone https://github.com/libsdl-org/SDL
cd libsdl-org/SDL
git checkout release-2.0.20 # the latest tag
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/home/zz/artifacts/sdl/2.0.20/linux-x64 -DCMAKE_BUILD_TYPE=Release
cmake --build .
cmake --install .
# Then later when building nn-editor project, pass `-D/home/zz/artifacts/sdl/2.0.20/linux-x64/lib/cmake/SDL2` to cmake.
```

- Another method:
    - `download the source from offcial website:http://www.libsdl.org/`
    ```bash
    wget https://www.libsdl.org/release/SDL2-2.0.18.zip
    ```

    - unzip the zip file
    - check the file and enter in the file root
    - use the command

    ```bash
    ./configure
    # or, specify custom install path:
    # ./configure --prefix=/some/custom/path
    make
    sudo make install
    ```

**Error in the make**
error1:yaml.h is missing
git clone https://github.com/jbeder/yaml-cpp
add the include file path ./3rdparty/yaml-cpp/include
error2:
undefined reference to symbol 'dlclose@@GLIBC_2.2.5'
add CMakeLists.txt link add **dl** library
