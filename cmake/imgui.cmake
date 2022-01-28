# clean platform and renderer settings
unset (IMGUI_PLATFORM)
unset (IMGUI_RENDERER)


# get user defined options
# check if platform was set
if (${PROJECT_NAME}_IMGUI_PLATFORM)
    set (IMGUI_PLATFORM ${${PROJECT_NAME}_IMGUI_PLATFORM})
endif()

# check if backend was set
if (${PROJECT_NAME}_IMGUI_RENDERER)
    set (IMGUI_RENDERER ${${PROJECT_NAME}_IMGUI_RENDERER})
endif()


# find some dependence
# TODO: we need "cmake version not about" finders
#       SDL2, GLFW, GLUT(guys who know this?)
#       Vulkan, Metal, OpenGL, WebGPU, DirectX
macro (CHECK_PACKAGES)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
    find_package(SDL2 QUIET)
    find_package(glfw3 QUIET)
    find_package(Vulkan QUIET)
endmacro()

# check platform
if (NOT IMGUI_PLATFORM)
    CHECK_PACKAGES()

    # dying new Big Blue
    if (WINDOWS_PHONE OR WINCE)
        if (WINCE)
            message (FATAL_ERROR "WinCE is not a supported platform for now...")
        endif()
        if (WINDOWS_PHONE)
            message (FATAL_ERROR "WINDOWS PHONE is not a supported platform for now...")
        endif()
    endif()

    # Note: MINGW not a win32 platform(needs double check)
    # TODO: add checking for absolutely sure the system is
    if (MINGW)
        if (GLFW_FOUND AND (NOT IMGUI_PLATFORM))
            set (IMGUI_PLATFORM GLFW)
        endif()

        # TODO: extract FindSDL.cmake for 3.19 lower cmake
        if (SDL2_FOUND AND (NOT IMGUI_PLATFORM))
            set (IMGUI_PLATFORM SDL2)
        endif()

        if (NOT IMGUI_PLATFORM)
            #message (FATAL_ERROR "MINGW is not a supported platform for now...")
        endif()
    endif()

    # TODO: how can i do with msys?
    if (WIN32) # which means win32, win64, msys
        # TODO: write a GLFW finder
        if (GLFW_FOUND AND (NOT IMGUI_PLATFORM))
            set (IMGUI_PLATFORM GLFW)
        endif()

        # TODO: extract FindSDL.cmake for 3.19 lower cmake
        if (SDL2_FOUND AND (NOT IMGUI_PLATFORM))
            set (IMGUI_PLATFORM SDL2)
        endif()

        if(NOT IMGUI_PLATFORM)
            set (IMGUI_PLATFORM WIN32)
        endif()
    endif()

    # Android, Google should pay for this
    # TODO: check NDK, make sure if backend always could be Vulkan(first NDK version)
    #       or keeps using OpenGL
    if (ANDROID)
        set (IMGUI_PLATFORM ANDROID)

        # TODO: will remove when backend insure
        message (FATAL_ERROR "Android is not a supported platform for now...")

        # default using OpenGL3 for Android (IMGUI backend opengl3 support OpenGL ES 2.0/3.0 both)
        if (NOT ${CMAKE_PROJECT_NAME}_IMGUI_RENDERER_FORCE_VULKAN)
            set (IMGUI_RENDERER OPENGL3)
        endif ()

        # some boards do not has a vulkan driver impl, need add some check
        if (NOT ${CMAKE_PROJECT_NAME}_IMGUI_RENDERER_FORCE_OPENGL)
            if (${CMAKE_PROJECT_NAME}_IMGUI_RENDERER_FORCE_VULKAN)
                set (IMGUI_RENDERER VULKAN)
                # must have some issue here
            elseif (CMAKE_ANDROID_NDK_VERSION VERSION_GREATER_EQUAL ?)
                # TODO: add more check
                set (IMGUI_RENDERER VULKAN)
            endif()
        endif()
    endif()

    # macOS, iOS, tvOS or watchOS
    if (APPLE)
        if (GLFW_FOUND AND (NOT IMGUI_PLATFORM))
            set (IMGUI_PLATFORM GLFW)
        endif()
        if (SDL2_FOUND AND (NOT IMGUI_PLATFORM))
            set (IMGUI_PLATFORM SDL2)
        endif()
        if(NOT IMGUI_PLATFORM)
            set (IMGUI_PLATFORM OSX)
        endif()

        # Bossy President does not like u: no choice for apple
        set (IMGUI_RENDERER METAL)
    endif()

    # Note: MINGW & APPLE should be earlier than UNIX
    if (UNIX AND (NOT (MINGW OR APPLE)))
        if (GLFW_FOUND AND (NOT IMGUI_PLATFORM))
            set (IMGUI_PLATFORM GLFW)
        endif()
        if (SDL2_FOUND AND (NOT IMGUI_PLATFORM))
            set (IMGUI_PLATFORM SDL2)
        endif()
        if(NOT IMGUI_PLATFORM)
            message (FATAL_ERROR "Unix-like system must install a supported platform(GLFW or SDL2) first...")
        endif()
    endif()

    # Green Hills MULTI, AUTOSAR users are very rich, pay for this
    # some kind-hearted millionaire give alms Green Hills IDE to us for developing this part
    if (GHS-MULTI)
        message (FATAL_ERROR "GHS-MULTI is not a supported platform for now...")
    endif()
endif()

if(NOT IMGUI_RENDERER)
    CHECK_PACKAGES()

    # (penguinliong) SDL2 platform MUST match SDL2 renderer.
    if (IMGUI_PLATFORM MATCHES SDL2)
        set (IMGUI_RENDERER SDL2)
    endif()

    if (Vulkan_FOUND AND (NOT IMGUI_RENDERER))
        set (IMGUI_RENDERER VULKAN)
    endif()

    # TODO: write DirectX Finder
    if (DirectX_FOUND AND (NOT IMGUI_RENDERER))
        if (DirectX_VERSION VERSION_GREATER_EQUAL 12)
            set (IMGUI_RENDERER DIRECTX12)
        endif()
        if (DirectX_VERSION VERSION_GREATER_EQUAL 11 AND (NOT IMGUI_RENDERER))
            set (IMGUI_RENDERER DIRECTX11)
        endif()
        if (DirectX_VERSION VERSION_GREATER_EQUAL 10 AND (NOT IMGUI_RENDERER))
            set (IMGUI_RENDERER DIRECTX10)
        endif()
        if (NOT IMGUI_RENDERER)
            set (IMGUI_RENDERER DIRECTX9)
        endif()
    elseif (IMGUI_PLATFORM MATCHES WIN32)
        if (NOT IMGUI_RENDERER)
            set (IMGUI_RENDERER OPENGL3)
        endif()
    endif()

    # TODO: write WebGPU Finder
    if (WebGPU_FOUND AND (NOT IMGUI_RENDERER))
        set (IMGUI_RENDERER SDL2)
    endif()

    # use SDL2 as a SDL2 based backup backend (if SDL2 was found)
    if (SDL2_FOUND AND (NOT IMGUI_RENDERER))
        set (IMGUI_RENDERER SDL2)
    endif()

    # use OpenGL as a backup backend
    if (NOT IMGUI_RENDERER)
        set (IMGUI_RENDERER OPENGL3)
    endif()

    # TODO: WebGPU was missed
endif()


# then, deal with sources

# add imported ImGUI
set (IMGUI_BASE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/imgui)

add_library (IMGUI INTERFACE IMPORTED)

target_include_directories (IMGUI INTERFACE ${IMGUI_BASE_DIR})
target_include_directories (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends)
target_include_directories (IMGUI INTERFACE ${IMGUI_BASE_DIR}/misc/cpp)

target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/imgui.cpp)
target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/imgui_demo.cpp)
target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/imgui_draw.cpp)
target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/imgui_tables.cpp)
target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/imgui_widgets.cpp)

target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/misc/cpp/imgui_stdlib.cpp)

# judge platform
if (IMGUI_PLATFORM)
    message("-- ImGui platform is " ${IMGUI_PLATFORM})

    if     (IMGUI_PLATFORM MATCHES WIN32)
        target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends/imgui_impl_win32.cpp)
    elseif (IMGUI_PLATFORM MATCHES SDL2)
        target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends/imgui_impl_sdl.cpp)
    elseif (IMGUI_PLATFORM MATCHES GLFW)
        target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends/imgui_impl_glfw.cpp)
    elseif (IMGUI_PLATFORM MATCHES ANDROID)
        target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends/imgui_impl_android.cpp)
    elseif (IMGUI_PLATFORM MATCHES OSX)
        enable_language(OBJCXX)
        set(CMAKE_OBJCCXX_FLAGS "-x objective-c++ -fobjc-weak")
        target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends/imgui_impl_osx.mm)
    endif()
else()
    message (FATAL_ERROR "IMGUI Platform is not set, is unscientific...")
endif ()

# judge renderer
if (IMGUI_RENDERER)
    message("-- ImGui renderer is " ${IMGUI_RENDERER})

    if     (IMGUI_RENDERER MATCHES VULKAN)
        target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends/imgui_impl_vulkan.cpp)
    elseif (IMGUI_RENDERER MATCHES SDL2)
        target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends/imgui_impl_sdlrenderer.cpp)
    elseif (IMGUI_RENDERER MATCHES METAL)
        target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends/imgui_impl_metal.mm)
    elseif (IMGUI_RENDERER MATCHES WEBGPU)
        target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends/imgui_impl_wgpu.cpp)
    elseif (IMGUI_RENDERER MATCHES OPENGL3)
        target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends/imgui_impl_opengl3.cpp)
    elseif (IMGUI_RENDERER MATCHES DIRECTX12)
        target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends/imgui_impl_dx12.cpp)
    elseif (IMGUI_RENDERER MATCHES DIRECTX11)
        target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends/imgui_impl_dx11.cpp)
    elseif (IMGUI_RENDERER MATCHES DIRECTX10)
        target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends/imgui_impl_dx10.cpp)
    elseif (IMGUI_RENDERER MATCHES DIRECTX9)
        target_sources (IMGUI INTERFACE ${IMGUI_BASE_DIR}/backends/imgui_impl_dx9.cpp)
    endif()
else()
    message (FATAL_ERROR "IMGUI Renderer is not set, is unscientific...")
endif()


# add C++ standard require
# disabled for others need a higher standard
#set_target_properties (IMGUI CXX_STANDARD 11)


# add headers and link libraries

# for platform
if     (IMGUI_PLATFORM MATCHES WIN32)
    # TODO: link what?
elseif (IMGUI_PLATFORM MATCHES SDL2)
    # TODO: (penguinliong) Remember to copy the SDL dynamic library to the
    # binary dir in a future update.
    target_link_libraries (IMGUI INTERFACE SDL2::SDL2)
elseif (IMGUI_PLATFORM MATCHES GLFW)
    # TODO: link what? header where?
elseif (IMGUI_PLATFORM MATCHES ANDROID)
    # TODO: link what? header where?
elseif (IMGUI_PLATFORM MATCHES OSX)
    # TODO: link what?
endif()

# for renderer
if     (IMGUI_RENDERER MATCHES VULKAN)
    target_include_directories(IMGUI INTERFACE ${Vulkan_INCLUDE_DIR})
    target_link_libraries (IMGUI INTERFACE ${Vulkan_LIBRARY})
elseif (IMGUI_RENDERER MATCHES SDL2)
    # TODO: this part may not work
    #target_link_libraries (IMGUI INTERFACE SDL2main)
elseif (IMGUI_RENDERER MATCHES METAL)
    # TODO: link what?
elseif (IMGUI_RENDERER MATCHES WEBGPU)
    # TODO: link what?
elseif (IMGUI_RENDERER MATCHES OPENGL3)
    if (IMGUI_PLATFORM MATCHES ANDROID)
        # TODO: check NDK for which version use OpenGL ESv2, or ESv3
        target_link_libraries (IMGUI INTERFACE GLESv3)
        # elseif (TODO)
    else ()
        target_link_libraries (IMGUI INTERFACE GL)
    endif()
elseif (IMGUI_RENDERER MATCHES DIRECTX12)
    # TODO: link what?
elseif (IMGUI_RENDERER MATCHES DIRECTX11)
    # TODO: link what?
elseif (IMGUI_RENDERER MATCHES DIRECTX10)
    # TODO: link what?
elseif (IMGUI_RENDERER MATCHES DIRECTX9)
    # TODO: link what?
endif()
