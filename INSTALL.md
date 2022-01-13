# How to Build in the Linux

Install required build dependecies:

- git
- cmake
- g++
- NB(Mr.sun NB library is very **NB**) [NB](https://github.com/scarsty/nb)
- [SDL2](https://github.com/libsdl-org/SDL)

- OpenGL
- [yaml-cpp](https://github.com/jbeder/yaml-cpp)

**Git clone the node-editor repo with submodule**

```
git clone https://github.com/scarsty/node-editor
cd node-editor
git submodule update --init 
```

**And you can use this to build this repo**

Please modify the **CMakeLists.txt**

About the IncludePath and link please make sure the yaml-cpp 

```
mkdir build
cd build
cmake ..
make
```

Please use apt (or yum or pacman or brew) to install SDL2 and yaml-cpp if you can.

**On MacOS**

```
mkdir build
cd build
cmake -DCMAKE_SYSTEM_PROCESSOR=x86_64 ..
make
```

**SDL2 install**

- `download the source from the offcial website:http://www.libsdl.org/`

```
wget https://www.libsdl.org/release/SDL2-2.0.18.zip
```

- unzip the zip file
- check the file and enter in the file root
- use the command

```
./configure
```

- make

```
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
