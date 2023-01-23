# FindGTK4.cmake
# Modified from <https://github.com/nemequ/gnome-cmake>
#
# CMake support for GTK+ 4.
#
# License:
#
#   Copyright (c) 2016 Evan Nemerson <evan@nemerson.com>
#
#   Permission is hereby granted, free of charge, to any person
#   obtaining a copy of this software and associated documentation
#   files (the "Software"), to deal in the Software without
#   restriction, including without limitation the rights to use, copy,
#   modify, merge, publish, distribute, sublicense, and/or sell copies
#   of the Software, and to permit persons to whom the Software is
#   furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be
#   included in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
#   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
#   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
#   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
#   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
#   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#   DEALINGS IN THE SOFTWARE.

find_package(PkgConfig)

set(GTK4_DEPS
  GIO
  ATK
  GDK3
  Pango
  Cairo
  GDKPixbuf
  Graphene
  )

if(PKG_CONFIG_FOUND)
  pkg_search_module(GTK4_PKG gtk4)
endif()

find_library(GTK4_LIBRARY gtk-4 HINTS ${GTK4_PKG_LIBRARY_DIRS})
set(GTK4 gtk-4)

if(GTK4_LIBRARY)
  add_library(${GTK4} SHARED IMPORTED)
  set_property(TARGET ${GTK4} PROPERTY IMPORTED_LOCATION "${GTK4_LIBRARY}")
  set_property(TARGET ${GTK4} PROPERTY INTERFACE_COMPILE_OPTIONS "${GTK4_PKG_CFLAGS_OTHER}")

  set(GTK4_INCLUDE_DIRS)

  find_path(GTK4_INCLUDE_DIR "gtk/gtk.h"
    HINTS ${GTK4_PKG_INCLUDE_DIRS})

  if(GTK4_INCLUDE_DIR)
    file(STRINGS "${GTK4_INCLUDE_DIR}/gtk/gtkversion.h" GTK4_MAJOR_VERSION REGEX "^#define GTK_MAJOR_VERSION +\\(?([0-9]+)\\)?$")
    string(REGEX REPLACE "^#define GTK_MAJOR_VERSION \\(?([0-9]+)\\)?$" "\\1" GTK4_MAJOR_VERSION "${GTK4_MAJOR_VERSION}")
    file(STRINGS "${GTK4_INCLUDE_DIR}/gtk/gtkversion.h" GTK4_MINOR_VERSION REGEX "^#define GTK_MINOR_VERSION +\\(?([0-9]+)\\)?$")
    string(REGEX REPLACE "^#define GTK_MINOR_VERSION \\(?([0-9]+)\\)?$" "\\1" GTK4_MINOR_VERSION "${GTK4_MINOR_VERSION}")
    file(STRINGS "${GTK4_INCLUDE_DIR}/gtk/gtkversion.h" GTK4_MICRO_VERSION REGEX "^#define GTK_MICRO_VERSION +\\(?([0-9]+)\\)?$")
    string(REGEX REPLACE "^#define GTK_MICRO_VERSION \\(?([0-9]+)\\)?$" "\\1" GTK4_MICRO_VERSION "${GTK4_MICRO_VERSION}")
    set(GTK4_VERSION "${GTK4_MAJOR_VERSION}.${GTK4_MINOR_VERSION}.${GTK4_MICRO_VERSION}")
    unset(GTK4_MAJOR_VERSION)
    unset(GTK4_MINOR_VERSION)
    unset(GTK4_MICRO_VERSION)

    list(APPEND GTK4_INCLUDE_DIRS ${GTK4_INCLUDE_DIR})
    set_property(TARGET ${GTK4} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${GTK4_INCLUDE_DIR}")
  endif()
endif()

set(GTK4_DEPS_FOUND_VARS)
foreach(gtk4_dep ${GTK4_DEPS})
  find_package(${gtk4_dep})

  list(APPEND GTK4_DEPS_FOUND_VARS "${gtk4_dep}_FOUND")
  list(APPEND GTK4_INCLUDE_DIRS ${${gtk4_dep}_INCLUDE_DIRS})

  set_property (TARGET "${GTK4}" APPEND PROPERTY INTERFACE_LINK_LIBRARIES "${${gtk4_dep}}")
endforeach(gtk4_dep)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GTK4
    REQUIRED_VARS
      GTK4_LIBRARY
      GTK4_INCLUDE_DIRS
      ${GTK4_DEPS_FOUND_VARS}
    VERSION_VAR
      GTK4_VERSION)

unset(GTK4_DEPS_FOUND_VARS)
