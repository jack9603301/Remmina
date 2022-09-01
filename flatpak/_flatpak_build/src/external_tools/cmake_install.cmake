# Install script for directory: /home/benoit/dev/Remmina/src/external_tools

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
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/app/share/remmina/external_tools/launcher.sh;/app/share/remmina/external_tools/functions.sh;/app/share/remmina/external_tools/remmina_filezilla_sftp.sh;/app/share/remmina/external_tools/remmina_filezilla_sftp_pki.sh;/app/share/remmina/external_tools/remmina_nslookup.sh;/app/share/remmina/external_tools/remmina_ping.sh;/app/share/remmina/external_tools/remmina_traceroute.sh")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/app/share/remmina/external_tools" TYPE PROGRAM FILES
    "/home/benoit/dev/Remmina/src/external_tools/launcher.sh"
    "/home/benoit/dev/Remmina/src/external_tools/functions.sh"
    "/home/benoit/dev/Remmina/src/external_tools/remmina_filezilla_sftp.sh"
    "/home/benoit/dev/Remmina/src/external_tools/remmina_filezilla_sftp_pki.sh"
    "/home/benoit/dev/Remmina/src/external_tools/remmina_nslookup.sh"
    "/home/benoit/dev/Remmina/src/external_tools/remmina_ping.sh"
    "/home/benoit/dev/Remmina/src/external_tools/remmina_traceroute.sh"
    )
endif()

