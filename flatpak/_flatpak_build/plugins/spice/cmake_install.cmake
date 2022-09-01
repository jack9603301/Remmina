# Install script for directory: /home/benoit/dev/Remmina/plugins/spice

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/app")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "RelWithDebInfo")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/app/lib/remmina/plugins/remmina-plugin-spice.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/app/lib/remmina/plugins/remmina-plugin-spice.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/app/lib/remmina/plugins/remmina-plugin-spice.so"
         RPATH "$ORIGIN/../lib:$ORIGIN/..")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/app/lib/remmina/plugins/remmina-plugin-spice.so")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/app/lib/remmina/plugins" TYPE SHARED_LIBRARY FILES "/home/benoit/dev/Remmina/flatpak/_flatpak_build/plugins/spice/remmina-plugin-spice.so")
  if(EXISTS "$ENV{DESTDIR}/app/lib/remmina/plugins/remmina-plugin-spice.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/app/lib/remmina/plugins/remmina-plugin-spice.so")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/app/lib/remmina/plugins/remmina-plugin-spice.so"
         OLD_RPATH ":::::::::::::::::::::::::"
         NEW_RPATH "$ORIGIN/../lib:$ORIGIN/..")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/app/lib/remmina/plugins/remmina-plugin-spice.so")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/app/share/icons/hicolor/scalable/emblems/org.remmina.Remmina-spice-ssh-symbolic.svg;/app/share/icons/hicolor/scalable/emblems/org.remmina.Remmina-spice-symbolic.svg")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/app/share/icons/hicolor/scalable/emblems" TYPE FILE FILES
    "/home/benoit/dev/Remmina/plugins/spice/scalable/emblems/org.remmina.Remmina-spice-ssh-symbolic.svg"
    "/home/benoit/dev/Remmina/plugins/spice/scalable/emblems/org.remmina.Remmina-spice-symbolic.svg"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  
        set(DESTDIR_VALUE "$ENV{DESTDIR}")
        if (NOT DESTDIR_VALUE)
            execute_process(COMMAND /usr/bin/gtk-update-icon-cache -qfit /app/share/icons/hicolor)
        endif()
        
endif()

