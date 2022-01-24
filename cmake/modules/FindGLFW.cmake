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
FindGLFW
---------

GLFW is an Open Source, multi-platform library for OpenGL, OpenGL
ES and Vulkan development on the desktop. It provides a simple
API for creating windows, contexts and surfaces, receiving input
and events.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` targets if GLFW has been found:

``GLFW::GLFW``
  The main GLFW library.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

  GLFW_FOUND          - "True" if GLFW was found
  GLFW_INCLUDE_DIRS   - include directories for GLFW
  GLFW_LIBRARIES      - link against this library to use GLFW
  GLFW_VERSION_STRING - the GLFW version (eg. 3.3)
  GLFW_VERSION_MAJOR  - The major version of the GLFW implementation
  GLFW_VERSION_MINOR  - The minor version of the GLFW implementation
  GLFW_VERSION_PATCH  - The patch version of the GLFW implementation

The module will also define these cache variables::

  GLFW_INCLUDE_DIR    - the GLFW include directory
  GLFW_LIBRARY        - the path to the GLFW library

Hints
^^^^^

The ``GLFW_ROOT`` environment variable optionally specifies the
location of the GLFW root directory for the given architecture.

#============================================================================]]

# deal with headers folder
find_path(GLFW_INCLUDE_DIR
    NAMES
        GLFW/glfw3.h
    HINTS
        ${GLFW_ROOT}
        ENV GLFW_ROOT
    PATHS
        /usr
        /usr/local
        /opt
    PATH_SUFFIXES
        include include/GL include/X11
    )

# temp searching path suffix
list(APPEND _GLFW_PATH_SUFFIX lib)

if (MINGW)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        list(APPEND _GLFW_PATH_SUFFIX lib-mingw-w64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        list(APPEND _GLFW_PATH_SUFFIX lib-mingw)
        list(APPEND _GLFW_PATH_SUFFIX lib-mingw-w32)
    endif()
elseif (WIN32)
    if (MSVC11 OR ((${MSVC_VERSION} GREATER_EQUAL 1700) AND (${MSVC_VERSION} LESS 1800)))
        list(APPEND _GLFW_PATH_SUFFIX lib-vc2012)
    elseif (MSVC12 OR ((${MSVC_VERSION} GREATER_EQUAL 1800) AND (${MSVC_VERSION} LESS 1900)))
        list(APPEND _GLFW_PATH_SUFFIX lib-vc2013)
    elseif (MSVC14 OR ((${MSVC_VERSION} GREATER_EQUAL 1900) AND (${MSVC_VERSION} LESS 1910)))
        list(APPEND _GLFW_PATH_SUFFIX lib-vc2015)
    elseif (MSVC14 OR ((${MSVC_VERSION} GREATER_EQUAL 1910) AND (${MSVC_VERSION} LESS 1920)))
        list(APPEND _GLFW_PATH_SUFFIX lib-vc2017)
    elseif (MSVC14 OR ((${MSVC_VERSION} GREATER_EQUAL 1920) AND (${MSVC_VERSION} LESS 1930)))
        list(APPEND _GLFW_PATH_SUFFIX lib-vc2019)
    elseif (MSVC14 OR ((${MSVC_VERSION} GREATER_EQUAL 1930) AND (${MSVC_VERSION} LESS 1940)))
        list(APPEND _GLFW_PATH_SUFFIX lib-vc2022)
    endif()
else ()
    list(APPEND _GLFW_PATH_SUFFIX lib64)
    list(APPEND _GLFW_PATH_SUFFIX ${CMAKE_LIBRARY_ARCHITECTURE})
    list(APPEND _GLFW_PATH_SUFFIX lib/${CMAKE_LIBRARY_ARCHITECTURE})
    list(APPEND _GLFW_PATH_SUFFIX local/lib)
    list(APPEND _GLFW_PATH_SUFFIX local/lib64)
    list(APPEND _GLFW_PATH_SUFFIX local/${CMAKE_LIBRARY_ARCHITECTURE})
endif()

# start to searching libglfw
find_library(GLFW_LIBRARY
    NAMES
        glfw
        glfw3
    PATHS
        /usr
        /usr/local
        /opt
    PATH_SUFFIXES
        ${_GLFW_PATH_SUFFIX}
    )

unset(_GLFW_PATH_SUFFIX)

# check version of GLFW
if(GLFW_INCLUDE_DIR AND EXISTS "${GLFW_INCLUDE_DIR}/GLFW/glfw3.h")
    file(STRINGS "${GLFW_INCLUDE_DIR}/GLFW/glfw3.h" GLFW_VERSION_MAJOR_LINE REGEX "^#define[ \t]+GLFW_VERSION_MAJOR[ \t]+[0-9]+$")
    file(STRINGS "${GLFW_INCLUDE_DIR}/GLFW/glfw3.h" GLFW_VERSION_MINOR_LINE REGEX "^#define[ \t]+GLFW_VERSION_MINOR[ \t]+[0-9]+$")
    file(STRINGS "${GLFW_INCLUDE_DIR}/GLFW/glfw3.h" GLFW_VERSION_PATCH_LINE REGEX "^#define[ \t]+GLFW_VERSION_REVISION[ \t]+[0-9]+$")

    string(REGEX REPLACE "^#define[ \t]+GLFW_VERSION_MAJOR[ \t]+([0-9]+)$" "\\1"    GLFW_VERSION_MAJOR "${GLFW_VERSION_MAJOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+GLFW_VERSION_MINOR[ \t]+([0-9]+)$" "\\1"    GLFW_VERSION_MINOR "${GLFW_VERSION_MINOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+GLFW_VERSION_REVISION[ \t]+([0-9]+)$" "\\1" GLFW_VERSION_PATCH "${GLFW_VERSION_PATCH_LINE}")

    set(GLFW_VERSION ${GLFW_VERSION_MAJOR}.${GLFW_VERSION_MINOR}.${GLFW_VERSION_PATCH})
    set(GLFW_VERSION_STRING ${GLFW_VERSION})

    unset(GLFW_VERSION_MAJOR_LINE)
    unset(GLFW_VERSION_MINOR_LINE)
    unset(GLFW_VERSION_PATCH_LINE)
endif()

# handle GLFW package finding events
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLFW
    REQUIRED_VARS
        GLFW_INCLUDE_DIR
        GLFW_LIBRARY
    VERSION_VAR
        GLFW_VERSION_STRING
    )

# add targets
if (GLFW_FOUND)
    # define the target named GLFW::GLFW
    if(NOT TARGET GLFW::GLFW)
        add_library(GLFW::GLFW UNKNOWN IMPORTED)
        set_target_properties(GLFW::GLFW PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${GLFW_INCLUDE_DIR})

        # Link against Cocoa on macOS.
        if(APPLE)
            set_property(TARGET SDL2::SDL2MAIN APPEND PROPERTY INTERFACE_LINK_OPTIONS -framework Cocoa)
        endif()
        if(MINGW)
            set_property(TARGET SDL2::SDL2MAIN APPEND PROPERTY INTERFACE_LINK_OPTIONS mingw32 -mwindows)
        endif()

        set_property(TARGET GLFW::GLFW APPEND PROPERTY IMPORTED_LOCATION ${GLFW_LIBRARY})
    endif()
endif()

mark_as_advanced(GLFW_INCLUDE_DIR GLFW_LIBRARY)
