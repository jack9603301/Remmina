find_package(PkgConfig)

set(Graphene_DEPS)

if(PKG_CONFIG_FOUND)
  pkg_search_module(Graphene_PKG graphene-1.0)
endif()


find_library(Graphene_LIBRARY graphene-1.0 HINTS ${Graphene_PKG_LIBRARY_DIRS})
set(Graphene graphene)

if(Graphene_LIBRARY)
  add_library(${Graphene} SHARED IMPORTED)
  set_property(TARGET ${Graphene} PROPERTY IMPORTED_LOCATION "${Graphene_LIBRARY}")
  set_property(TARGET ${Graphene} PROPERTY INTERFACE_COMPILE_OPTIONS "${Graphene_PKG_CFLAGS_OTHER}")

  set(Graphene_INCLUDE_DIRS)
  find_path(Graphene_INCLUDE_DIR "graphene.h"
    HINTS ${Graphene_PKG_INCLUDE_DIRS})

  find_path(Graphene_CONFIG_DIR "graphene-config.h"
    HINTS ${Graphene_PKG_INCLUDE_DIRS})

  if(Graphene_INCLUDE_DIR)
    file(STRINGS "${Graphene_INCLUDE_DIR}/graphene-version.h" Graphene_VERSION_MAJOR REGEX "^#define Graphene_VERSION_MAJOR +\\(?([0-9]+)\\)?$")
    string(REGEX REPLACE "^#define Graphene_VERSION_MAJOR \\(?([0-9]+)\\)?$" "\\1" Graphene_VERSION_MAJOR "${Graphene_VERSION_MAJOR}")
    file(STRINGS "${Graphene_INCLUDE_DIR}/graphene-version.h" Graphene_VERSION_MINOR REGEX "^#define Graphene_VERSION_MINOR +\\(?([0-9]+)\\)?$")
    string(REGEX REPLACE "^#define Graphene_VERSION_MINOR \\(?([0-9]+)\\)?$" "\\1" Graphene_VERSION_MINOR "${Graphene_VERSION_MINOR}")
    file(STRINGS "${Graphene_INCLUDE_DIR}/graphene-version.h" Graphene_VERSION_MICRO REGEX "^#define Graphene_VERSION_MICRO +\\(?([0-9]+)\\)?$")
    string(REGEX REPLACE "^#define Graphene_VERSION_MICRO \\(?([0-9]+)\\)?$" "\\1" Graphene_VERSION_MICRO "${Graphene_VERSION_MICRO}")
    set(Graphene_VERSION "${Graphene_VERSION_MAJOR}.${Graphene_VERSION_MINOR}.${Graphene_VERSION_MICRO}")
    unset(Graphene_VERSION_MAJOR)
    unset(Graphene_VERSION_MINOR)
    unset(Graphene_VERSION_MICRO)

    list(APPEND Graphene_INCLUDE_DIRS ${Graphene_INCLUDE_DIR})
    list(APPEND Graphene_INCLUDE_DIRS ${Graphene_CONFIG_DIR})
    set_property(TARGET ${Graphene} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${Graphene_INCLUDE_DIR}")
  endif()
endif()

set(Graphene_DEPS_FOUND_VARS)
foreach(graphene_dep ${Graphene_DEPS})
  find_package(${graphene_dep})

  list(APPEND Graphene_DEPS_FOUND_VARS "${graphene_dep}_FOUND")
  list(APPEND Graphene_INCLUDE_DIRS ${${graphene_dep}_INCLUDE_DIRS})

  set_property (TARGET ${Graphene} APPEND PROPERTY INTERFACE_LINK_LIBRARIES "${${graphene_dep}}")
endforeach(graphene_dep)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Graphene
    REQUIRED_VARS
      Graphene_LIBRARY
      Graphene_INCLUDE_DIRS
      ${Graphene_DEPS_FOUND_VARS}
    VERSION_VAR
      Graphene_VERSION)

unset(Graphene_DEPS_FOUND_VARS)

