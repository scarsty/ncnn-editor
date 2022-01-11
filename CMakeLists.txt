project(node-editor)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_COMPILER $ENV{CXX})
set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -DNDEBUG")

include_directories(/usr/include/SDL2 ./3rdparty/nb /usr/local/include . ./3rdparty/imgui ./example ./3rdparty/imgui/backends ./3rdparty/imgui/misc/cpp ./3rdparty/imnodes ./3rdparty/ImGuiFileDialog)
link_directories(/usr/local/lib)

add_executable(node-editor
    3rdparty/imgui/imgui.cpp
    3rdparty/imgui/imgui_demo.cpp
    3rdparty/imgui/imgui_draw.cpp
    3rdparty/imgui/imgui_tables.cpp
    3rdparty/imgui/imgui_widgets.cpp
    3rdparty/imgui/backends/imgui_impl_opengl3.cpp
    3rdparty/imgui/backends/imgui_impl_sdl.cpp
    3rdparty/imgui/misc/cpp/imgui_stdlib.cpp
    node-editor/color_node_editor.cpp
    node-editor/main.cpp
    node-editor/loader.cpp
    node-editor/ccccloader.cpp
    node-editor/ncnnloader.cpp
    imnodes2/imnodes.cpp
    3rdparty/nb/convert.cpp
    3rdparty/nb/File.cpp
    3rdparty/ImGuiFileDialog/ImGuiFileDialog.cpp)
target_link_libraries(node-editor SDL2 SDL2main GL dl)

install(TARGETS node-editor RUNTIME DESTINATION bin)
