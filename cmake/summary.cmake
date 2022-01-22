# C/C++ compiler report

message(STATUS "")
message(STATUS "")
message(STATUS "Information Summary:")
message(STATUS "")
# CMake information
message(STATUS "CMake information:")
message(STATUS "  - CMake version:              ${CMAKE_VERSION}")
message(STATUS "  - CMake generator:            ${CMAKE_GENERATOR}")
message(STATUS "  - CMake building tools:       ${CMAKE_BUILD_TOOL}")
message(STATUS "  - Target System:              ${CMAKE_SYSTEM_NAME}")
message(STATUS "")


# C/C++ compiler information
message(STATUS "${PROJECT_NAME} toolchain information:")
message(STATUS "  Cross compiling: ${CMAKE_CROSSCOMPILING}")
message(STATUS "  C/C++ compiler:")
message(STATUS "    - C   standard version:     C${CMAKE_C_STANDARD}")
message(STATUS "    - C   standard required:    ${CMAKE_C_STANDARD_REQUIRED}")
message(STATUS "    - C   standard extensions:  ${CMAKE_C_EXTENSIONS}")
message(STATUS "    - C   compiler version:     ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "    - C   compiler:             ${CMAKE_C_COMPILER}")
message(STATUS "    - C++ standard version:     C++${CMAKE_CXX_STANDARD}")
message(STATUS "    - C++ standard required:    ${CMAKE_CXX_STANDARD_REQUIRED}")
message(STATUS "    - C++ standard extensions:  ${CMAKE_CXX_EXTENSIONS}")
message(STATUS "    - C++ compiler version:     ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "    - C++ compiler:             ${CMAKE_CXX_COMPILER}")
message(STATUS "  C/C++ compiler flags:")
message(STATUS "    - C   compiler flags:       ${CMAKE_C_FLAGS}")
message(STATUS "    - C++ compiler flags:       ${CMAKE_CXX_FLAGS}")
if(APPLE)
    message(STATUS "    - ObjC++ compiler version:  ${CMAKE_OBJCXX_COMPILER_VERSION}")
    message(STATUS "    - ObjC++ compiler:          ${CMAKE_OBJCXX_COMPILER}")
    message(STATUS "    - ObjC++ compiler flags:    ${CMAKE_OBJCXX_FLAGS}")
endif()

message(STATUS "  OpenMP:")
if(OpenMP_FOUND)
    message(STATUS "    - OpenMP was found:         YES")
    message(STATUS "    - OpenMP version:           ${OpenMP_C_VERSION}")
else()
    message(STATUS "    - OpenMP was found:         NO")
endif()
    message(STATUS "")


# CMake project information
message(STATUS "${PROJECT_NAME} building information:")
message(STATUS "  - Project source path is:     ${PROJECT_SOURCE_DIR}")
message(STATUS "  - Project building path is:   ${CMAKE_BINARY_DIR}")
message(STATUS "")


message(STATUS "${PROJECT_NAME} other information:")
# show building install path
message(STATUS "  IMGUI Platform:               ${IMGUI_PLATFORM}")
message(STATUS "  IMGUI Renderer:               ${IMGUI_RENDERER}")
if(SDL2_FOUND)
    message(STATUS "    - SDL2 version:             ${SDL2_VERSION}")
endif()
if(GLFW_FOUND)
    message(STATUS "    - GLFW version:             ${GLFW_VERSION}")
endif()
message(STATUS "  Package install path:         ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
