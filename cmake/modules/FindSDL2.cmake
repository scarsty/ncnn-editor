# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# License); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# AS IS BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
# Author: LS.Wang <314377460@qq.com>
#

#[[============================================================================
FindSDL2
---------

SDL2(Simple DirectMedia Layer) is a cross-platform development
library designed to provide low level access to audio, keyboard,
mouse, joystick, and graphics hardware via OpenGL and Direct3D.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` targets if SDL2 has been found:

``SDL2::SDL2``
  The main SDL2 library.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

  SDL2_FOUND          - "True" if SDL2 was found
  SDL2_INCLUDE_DIRS   - include directories for SDL2
  SDL2_LIBRARIES      - link against this library to use SDL2
  SDL2_VERSION        - the SDL2 version (eg. 2.0)
  SDL2_VERSION_MAJOR  - The major version of the SDL2 implementation
  SDL2_VERSION_MINOR  - The minor version of the SDL2 implementation
  SDL2_VERSION_PATCH  - The patch version of the SDL2 implementation

The module will also define these cache variables::

  SDL2_INCLUDE_DIR    - the SDL2 include directory
  SDL2_LIBRARY        - the path to the SDL2 library

Hints
^^^^^

The ``SDL2_ROOT`` environment variable optionally specifies the
location of the SDL2 root directory for the given architecture.

#============================================================================]]

# deal with headers folder
find_path(SDL2_INCLUDE_DIR
    NAMES
        SDL.h
    HINTS
        ${SDL2_ROOT}
        ${SDL2_PATH}
        ENV SDLDIR
        ENV SDL2_ROOT
        ENV SDL2_PATH
    PATHS
        /usr/include
        /usr/include/GL
        /usr/include/X11
        /usr/local/include
    PATH_SUFFIXES
        SDL2 include include/SDL2
    )

# temp searching path suffix
list(APPEND _SDL2_PATH_SUFFIX lib)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    list(APPEND _SDL2_PATH_SUFFIX lib/x64)
    list(APPEND _SDL2_PATH_SUFFIX lib/x86_64-linux-gnu)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    list(APPEND _SDL2_PATH_SUFFIX lib/x86)
endif()

# SDL-2.0 is the name used by FreeBSD ports...
# don't confuse it for the version number.
find_library(SDL2_LIBRARY_TEMP
    NAMES
        SDL2
        SDL-2.0
    HINTS
        ${SDL2_ROOT}
        ${SDL2_PATH}
        ENV SDLDIR
        ENV SDL2_ROOT
        ENV SDL2_PATH
    PATHS
        /usr
        /usr/local
        /opt
    PATH_SUFFIXES
        ${_SDL2_PATH_SUFFIX}
    )

# Hide this cache variable from the user, it's an internal implementation
# detail. The documented library variable for the user is SDL2_LIBRARY
# which is derived from SDL2_LIBRARY_TEMP further below.
set_property(CACHE SDL2_LIBRARY_TEMP PROPERTY TYPE INTERNAL)

if(NOT SDL2_INCLUDE_DIR MATCHES ".framework")
    # Non-OS X framework versions expect you to also dynamically link to
    # SDL2main. This is mainly for Windows and OS X. Other (Unix) platforms
    # seem to provide SDL2main for compatibility even though they don't
    # necessarily need it.
    find_library(SDL2MAIN_LIBRARY
        NAMES
            SDL2main
            SDL2main-2.0
        HINTS
            ${SDL2_ROOT}
            ${SDL2_PATH}
            ENV SDLDIR
            ENV SDL2_ROOT
            ENV SDL2_PATH
        PATHS
            /usr
            /usr/local
            /opt
        PATH_SUFFIXES
            ${_SDL2_PATH_SUFFIX}
        )
endif()

unset(_SDL2_PATH_SUFFIX)

# SDL2 may require threads on your system.
# The Apple build may not need an explicit flag because one of the
# frameworks may already provide it.
# But for non-OSX systems, I will use the CMake Threads package.
if(NOT APPLE)
    find_package(Threads)
endif()

# MinGW needs an additional link flag, -mwindows
# It's total link flags should look like -lmingw32 -lSDL2main -lSDL2 -mwindows
if(MINGW)
    set(MINGW32_LIBRARY mingw32 "-mwindows" CACHE STRING "link flags for MinGW")
endif()

if(SDL2_LIBRARY_TEMP)
    # For SDL2main
    if(SDL2MAIN_LIBRARY)
        list(FIND SDL2_LIBRARY_TEMP "${SDL2MAIN_LIBRARY}" _SDL2_MAIN_INDEX)
        if(_SDL2_MAIN_INDEX EQUAL -1)
            set(SDL2_LIBRARY_TEMP "${SDL2MAIN_LIBRARY}" ${SDL2_LIBRARY_TEMP})
        endif()
        unset(_SDL2_MAIN_INDEX)
    endif()

    # For OS X, SDL2 uses Cocoa as a backend so it must link to Cocoa.
    # CMake doesn't display the -framework Cocoa string in the UI even
    # though it actually is there if I modify a pre-used variable.
    # I think it has something to do with the CACHE STRING.
    # So I use a temporary variable until the end so I can set the
    # "real" variable in one-shot.
    if(APPLE)
        set(SDL2_LIBRARY_TEMP ${SDL2_LIBRARY_TEMP} "-framework Cocoa")
    endif()

    # For threads, as mentioned Apple doesn't need this.
    # In fact, there seems to be a problem if I used the Threads package
    # and try using this line, so I'm just skipping it entirely for OS X.
    if(NOT APPLE)
        set(SDL2_LIBRARY_TEMP ${SDL2_LIBRARY_TEMP} ${CMAKE_THREAD_LIBS_INIT})
    endif()

    # For MinGW library
    if(MINGW)
        set(SDL2_LIBRARY_TEMP ${MINGW32_LIBRARY} ${SDL2_LIBRARY_TEMP})
    endif()

    # Set the final string here so the GUI reflects the final state.
    set(SDL2_LIBRARY ${SDL2_LIBRARY_TEMP} CACHE STRING "Where the SDL2 Library can be found")
endif()

# checkout version from header
if(SDL2_INCLUDE_DIR AND EXISTS "${SDL2_INCLUDE_DIR}/SDL_version.h")
    file(STRINGS "${SDL2_INCLUDE_DIR}/SDL_version.h" SDL2_VERSION_MAJOR_LINE REGEX "^#define[ \t]+SDL_MAJOR_VERSION[ \t]+[0-9]+$")
    file(STRINGS "${SDL2_INCLUDE_DIR}/SDL_version.h" SDL2_VERSION_MINOR_LINE REGEX "^#define[ \t]+SDL_MINOR_VERSION[ \t]+[0-9]+$")
    file(STRINGS "${SDL2_INCLUDE_DIR}/SDL_version.h" SDL2_VERSION_PATCH_LINE REGEX "^#define[ \t]+SDL_PATCHLEVEL[ \t]+[0-9]+$")

    string(REGEX REPLACE "^#define[ \t]+SDL_MAJOR_VERSION[ \t]+([0-9]+)$" "\\1" SDL2_VERSION_MAJOR "${SDL2_VERSION_MAJOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+SDL_MINOR_VERSION[ \t]+([0-9]+)$" "\\1" SDL2_VERSION_MINOR "${SDL2_VERSION_MINOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+SDL_PATCHLEVEL[ \t]+([0-9]+)$" "\\1"    SDL2_VERSION_PATCH "${SDL2_VERSION_PATCH_LINE}")

    set(SDL2_VERSION ${SDL2_VERSION_MAJOR}.${SDL2_VERSION_MINOR}.${SDL2_VERSION_PATCH})
    set(SDL2_VERSION_STRING ${SDL2_VERSION})

    unset(SDL2_VERSION_MAJOR_LINE)
    unset(SDL2_VERSION_MINOR_LINE)
    unset(SDL2_VERSION_PATCH_LINE)
endif()

# handle SDL2 package finding events
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2
    REQUIRED_VARS
        SDL2_INCLUDE_DIR
        SDL2_LIBRARY
    VERSION_VAR
        SDL2_VERSION_STRING
    )

# add targets
if (SDL2_FOUND)
    set(SDL2_LIBRARIES ${SDL2_LIBRARY})
    set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})

    # define the target named SDL2::SDL2
    if(NOT TARGET SDL2::SDL2)
        add_library(SDL2::SDL2 UNKNOWN IMPORTED)
        set_target_properties(SDL2::SDL2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${SDL2_INCLUDE_DIR})
        set_target_properties(SDL2::SDL2 PROPERTIES INTERFACE_LINK_LIBRARIES "${SDL2_LIBRARY}")
        set_property(TARGET SDL2::SDL2 APPEND PROPERTY IMPORTED_LOCATION ${SDL2_LIBRARY})
    endif()
endif()

mark_as_advanced(SDL2_INCLUDE_DIR SDL2_LIBRARY)
