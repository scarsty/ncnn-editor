project(node-editor)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_COMPILER $ENV{CXX})
set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -DNDEBUG")

include_directories(/usr/include/SDL2 ../nb /usr/local/include . ./imgui ./example ./imgui/backends ./imgui/misc/cpp ./imnodes ./ImGuiFileDialog)
link_directories(/usr/local/lib)

if(WIN32)
    add_definitions(-DWIN32)
    SET(CMAKE_INCLUDE_DIRECTORIES_BEFORE ON) #  
    SET(SDL2_DIR "./SDL2")
    find_file(SDL2_INCLUDE_DIR NAME SDL.h HINTS ${SDL2_DIR} PATH_SUFFIXES include REQUIRED)
    get_filename_component(SDL2_INCLUDE_DIR ${SDL2_INCLUDE_DIR} DIRECTORY)
    find_library(SDL2_LIBRARY NAME SDL2.lib  HINTS ${SDL2_DIR}  PATH_SUFFIXES lib/x64 REQUIRED)
    message(STATUS "SDL2_INCLUDE_DIR = ${SDL2_INCLUDE_DIR}"  )
    include_directories("${SDL2_INCLUDE_DIR}")
    get_filename_component(SDL2_LIBRARY ${SDL2_LIBRARY} DIRECTORY)
    message(STATUS "SDL2_LIBRARY = ${SDL2_LIBRARY}"  )
    link_directories("${SDL2_LIBRARY}")
    set(GL opengl32)
else()
    find_package(SDL2 REQUIRED)
    set(GL GL)
endif()

include_directories(nb)


add_executable(node-editor
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_opengl3.cpp
    imgui/backends/imgui_impl_sdl.cpp
    imgui/misc/cpp/imgui_stdlib.cpp
    node-editor/color_node_editor.cpp
    node-editor/main.cpp
    node-editor/loader.cpp
    node-editor/ccccloader.cpp
    node-editor/ncnnloader.cpp
    node-editor/yamlyoloLoader.cpp
    imnodes2/imnodes.cpp
    nb/convert.cpp
    nb/File.cpp
    ImGuiFileDialog/ImGuiFileDialog.cpp)


target_link_libraries(node-editor SDL2 SDL2main ${GL})

install(TARGETS node-editor RUNTIME DESTINATION bin)