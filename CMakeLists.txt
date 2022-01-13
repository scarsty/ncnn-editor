# check cmake version first
cmake_minimum_required(VERSION 3.13)

# disable inplace builds to prevent source tree corruption.
# if (" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
#     message (FATAL_ERROR "Building inplace are not allowed. You should create a separate directory.")
# endif ()

# set cmake_install_prefix path
if (NOT DEFINED CMAKE_INSTALL_PREFIX)
    set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Installation directory")
endif()

# project name
project(node-editor)

# import IMGUI
include(${CMAKE_SOURCE_DIR}/cmake/imgui.cmake)

# add a target
add_executable(${PROJECT_NAME})

set_target_properties (${PROJECT_NAME} PROPERTIES COMPILE_DEFINITIONS NDEBUG)
set_target_properties (${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

# collect the source files
if(APPLE)
    add_compile_options(-x objective-c++ -fobjc-weak)
    set(CMAKE_EXE_LINKER_FLAGS "-framework Metal -framework QuartzCore")
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_FRAMEWORK_IDENTIFIER org.cmake.node-editor)
    list (APPEND NODE_EDITOR_SRCS node-editor/color_node_editor.cpp)
    list (APPEND NODE_EDITOR_SRCS node-editor/main.mm)
    list (APPEND NODE_EDITOR_SRCS node-editor/loader.cpp)
    list (APPEND NODE_EDITOR_SRCS node-editor/ccccloader.cpp)
    list (APPEND NODE_EDITOR_SRCS node-editor/ncnnloader.cpp)
else()
    list (APPEND NODE_EDITOR_SRCS node-editor/color_node_editor.cpp)
    list (APPEND NODE_EDITOR_SRCS node-editor/main.mm)
    list (APPEND NODE_EDITOR_SRCS node-editor/loader.cpp)
    list (APPEND NODE_EDITOR_SRCS node-editor/ccccloader.cpp)
    list (APPEND NODE_EDITOR_SRCS node-editor/ncnnloader.cpp)
endif(APPLE)

aux_source_directory(${CMAKE_SOURCE_DIR}/imnodes2    NODE_EDITOR_NODE_SRCS)
list (APPEND NODE_EDITOR_NB_SRCS 3rdparty/nb/convert.cpp)
list (APPEND NODE_EDITOR_NB_SRCS 3rdparty/nb/File.cpp)
list (APPEND NODE_EDITOR_DIALOG_SRCS 3rdparty/ImGuiFileDialog/ImGuiFileDialog.cpp)

target_sources (${PROJECT_NAME} PRIVATE ${NODE_EDITOR_SRCS})
target_sources (${PROJECT_NAME} PRIVATE ${NODE_EDITOR_NODE_SRCS})
target_sources (${PROJECT_NAME} PRIVATE ${NODE_EDITOR_NB_SRCS})
target_sources (${PROJECT_NAME} PRIVATE ${NODE_EDITOR_DIALOG_SRCS})

target_include_directories (${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/example)
target_include_directories (${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/3rdparty/nb)
target_include_directories (${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/3rdparty/imnodes)
target_include_directories (${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/3rdparty/ImGuiFileDialog)

target_link_libraries (${PROJECT_NAME} PRIVATE IMGUI)
find_package(SDL2)
target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_LIBRARIES})

if (UNIX)
    target_link_directories (${PROJECT_NAME} PRIVATE /usr/local/lib)
    target_link_libraries (${PROJECT_NAME} PRIVATE dl)
endif ()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

# summary
include(${CMAKE_SOURCE_DIR}/cmake/summary.cmake)
