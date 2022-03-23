# Install script for directory: /home/mathias/Projects/Remmina/data/icons

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
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
  set(CMAKE_INSTALL_SO_NO_EXE "1")
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
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-camera-photo-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-connect-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-disconnect-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-document-save-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-document-send-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-duplicate-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-dynres-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-fit-window-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-fullscreen-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-multi-monitor-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-go-bottom-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-keyboard-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-pan-down-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-pan-up-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-pin-down-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-pin-up-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-preferences-system-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-scale-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-switch-page-symbolic.svg;/usr/local/share/icons/hicolor/scalable/actions/org.remmina.Remmina-system-run-symbolic.svg")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/share/icons/hicolor/scalable/actions" TYPE FILE FILES
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-camera-photo-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-connect-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-disconnect-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-document-save-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-document-send-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-duplicate-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-dynres-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-fit-window-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-fullscreen-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-multi-monitor-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-go-bottom-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-keyboard-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-pan-down-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-pan-up-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-pin-down-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-pin-up-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-preferences-system-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-scale-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-switch-page-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/actions/org.remmina.Remmina-system-run-symbolic.svg"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/share/icons/hicolor/scalable/emblems/org.remmina.Remmina-sftp-symbolic.svg;/usr/local/share/icons/hicolor/scalable/emblems/org.remmina.Remmina-ssh-symbolic.svg")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/share/icons/hicolor/scalable/emblems" TYPE FILE FILES
    "/home/mathias/Projects/Remmina/data/icons/scalable/emblems/org.remmina.Remmina-sftp-symbolic.svg"
    "/home/mathias/Projects/Remmina/data/icons/scalable/emblems/org.remmina.Remmina-ssh-symbolic.svg"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  
        set(DESTDIR_VALUE "$ENV{DESTDIR}")
        if (NOT DESTDIR_VALUE)
            execute_process(COMMAND /usr/bin/gtk-update-icon-cache -qfit /usr/local/share/icons/hicolor)
        endif()
        
endif()

