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
project(nn-editor)

option(NETEDIT_WEB "build browser version with Emscripten" OFF)

option(NETEDIT_LOADER_CCCC "support cccc model loader" ON)
option(NETEDIT_LOADER_NCNN "support ncnn model loader" ON)
option(NETEDIT_LOADER_YAML_YOLO "support yaml yolo model loader" ON)

# import IMGUI
include(${CMAKE_SOURCE_DIR}/cmake/imgui.cmake)

# overlook, take care of severe warnings
# include(${CMAKE_SOURCE_DIR}/cmake/overlook.cmake)

# add a target
add_executable(${PROJECT_NAME})

set_target_properties (${PROJECT_NAME} PROPERTIES COMPILE_DEFINITIONS NDEBUG)
set_target_properties (${PROJECT_NAME} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

if (EMSCRIPTEN OR NETEDIT_WEB)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "index")
endif()


# collect the source files
list (APPEND NODE_EDITOR_SRCS nn-editor/nn_editor.cpp)
# list (APPEND NODE_EDITOR_SRCS nn-editor/nodeloader.cpp)
list (APPEND NODE_EDITOR_SRCS nn-editor/FileLoader.cpp)
# cccc is always loaded. qing zhi chi cccc.
list (APPEND NODE_EDITOR_SRCS nn-editor/ccccloader.cpp)
add_definitions(-DNETEDIT_LOADER_CCCC)

if(NETEDIT_LOADER_NCNN)
    list (APPEND NODE_EDITOR_SRCS nn-editor/ncnnloader.cpp)
    add_definitions(-DNETEDIT_LOADER_NCNN)
endif()

if(NETEDIT_LOADER_YAML_YOLO)
    set(NETEDIT_HAS_YAML_CPP OFF)

    # Prefer config package (works well with vcpkg, including wasm32-emscripten triplet).
    find_package(yaml-cpp CONFIG QUIET)
    if(TARGET yaml-cpp::yaml-cpp)
        set(NETEDIT_HAS_YAML_CPP ON)
    elseif(NOT (EMSCRIPTEN OR NETEDIT_WEB))
        # Backward compatible fallback for legacy desktop setups.
        find_package(YAML-CPP QUIET)
        if(YAML_CPP_LIBRARIES)
            set(NETEDIT_HAS_YAML_CPP ON)
        endif()
    endif()

    if(NETEDIT_HAS_YAML_CPP)
        list (APPEND NODE_EDITOR_SRCS nn-editor/yamlyololoader.cpp)
        add_definitions(-DNETEDIT_LOADER_YAML_YOLO)
        target_compile_definitions(${PROJECT_NAME} PRIVATE NETEDIT_HAS_YAML_CPP)

        if(TARGET yaml-cpp::yaml-cpp)
            target_link_libraries(${PROJECT_NAME} PRIVATE yaml-cpp::yaml-cpp)
        else()
            target_include_directories(${PROJECT_NAME} PRIVATE ${YAML_CPP_INCLUDE_DIRS})
            target_link_libraries(${PROJECT_NAME} PRIVATE ${YAML_CPP_LIBRARIES})
        endif()
    else()
        message(STATUS "Disabling YAML YOLO loader: yaml-cpp not found for current toolchain")
    endif()
endif()

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES ${CMAKE_SYSTEM_PROCESSOR})
    enable_language(OBJCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-weak -Wunused-command-line-argument")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -x objective-c++ -fobjc-weak -Wunused-command-line-argument")
    set(CMAKE_EXE_LINKER_FLAGS "-framework Metal -framework MetalKit -framework Foundation -framework Cocoa -framework Carbon -framework GameController")
    set(RESOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/nn-editor/ncnn-metadata.json)
    target_sources(${PROJECT_NAME} PRIVATE ${RESOURCE_FILES})
    set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/nn-editor/Info.plist
    RESOURCE "${RESOURCE_FILES}")
    list (APPEND NODE_EDITOR_SRCS nn-editor/main.mm)
else()
    list (APPEND NODE_EDITOR_SRCS nn-editor/main.cpp)
endif(APPLE)

aux_source_directory(${CMAKE_SOURCE_DIR}/imnodes2    NODE_EDITOR_NODE_SRCS)
list (APPEND NODE_EDITOR_NB_SRCS 3rdparty/mlcc/filefunc.cpp)
list (APPEND NODE_EDITOR_NB_SRCS 3rdparty/mlcc/strfunc.cpp)
list (APPEND NODE_EDITOR_DIALOG_SRCS 3rdparty/ImGuiFileDialog/ImGuiFileDialog.cpp)

target_sources (${PROJECT_NAME} PRIVATE ${NODE_EDITOR_SRCS})
target_sources (${PROJECT_NAME} PRIVATE ${NODE_EDITOR_NODE_SRCS})
target_sources (${PROJECT_NAME} PRIVATE ${NODE_EDITOR_NB_SRCS})
target_sources (${PROJECT_NAME} PRIVATE ${NODE_EDITOR_DIALOG_SRCS})

target_include_directories (${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/example)
target_include_directories (${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/3rdparty/mlcc)
target_include_directories (${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/3rdparty/imnodes)
target_include_directories (${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/3rdparty/ImGuiFileDialog)

target_link_libraries (${PROJECT_NAME} PRIVATE IMGUI)
if(EMSCRIPTEN OR NETEDIT_WEB)
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_ES3 USE_OPENGL3)
    target_compile_options(${PROJECT_NAME} PRIVATE
        "-sUSE_SDL=3"
        "-sWASM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFORCE_FILESYSTEM=1"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2")
    target_link_options(${PROJECT_NAME} PRIVATE
        "-sUSE_SDL=3"
        "-sWASM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFORCE_FILESYSTEM=1"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "--shell-file=${CMAKE_SOURCE_DIR}/nn-editor/web-shell.html"
        "--preload-file=${CMAKE_SOURCE_DIR}/models/squeezenet_v1.1.param@/models/squeezenet_v1.1.param"
        "--preload-file=${CMAKE_SOURCE_DIR}/nn-editor/ncnn-metadata.json@/ncnn-metadata.json"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','FS']"
        "-sEXPORTED_FUNCTIONS=['_main','_NodeEditorSetPendingFile']")
elseif(NOT APPLE)
    if(IMGUI_RENDERER MATCHES OPENGL3)
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_OPENGL3)
    endif()
    set ( CMAKE_PREFIX_PATH  "SDL3-PATH")
    find_package(SDL3 REQUIRED)
    target_include_directories(${PROJECT_NAME} PRIVATE ${SDL3_INCLUDE_DIRS} "./3rdparty/mlcc")
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL3_LIBRARIES})
endif()


if (UNIX)
    target_link_directories (${PROJECT_NAME} PRIVATE /usr/local/lib)
    target_link_libraries (${PROJECT_NAME} PRIVATE dl)
endif ()

if(APPLE)
    install(TARGETS ${PROJECT_NAME} BUNDLE DESTINATION bin)
else()
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
endif(APPLE)



# summary
include(${CMAKE_SOURCE_DIR}/cmake/summary.cmake)
