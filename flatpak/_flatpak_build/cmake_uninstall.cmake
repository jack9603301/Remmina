# 'delete_empty_folder' function
function(delete_empty_folder DIR)
    if(NOT EXISTS ${DIR})
        return()
    endif()

    # check if folder is empty
    file(GLOB RESULT "${DIR}/*")
    list(LENGTH RESULT RES_LEN)
    if(RES_LEN EQUAL 0)
        if(WITH_CLEANUP)
            message(STATUS "Deleting empty folder ${DIR}")
            file(REMOVE_RECURSE ${DIR})
        else()
            message(STATUS " ${DIR}")
        endif()
    else()
        #message(STATUS "${DIR} is not empty! It won't be removed.")
        # message(STATUS "FILES = ${RESULT}")
    endif()
endfunction(delete_empty_folder)

if( NOT EXISTS "/home/benoit/dev/Remmina/flatpak/_flatpak_build/install_manifest.txt" )
    message(FATAL_ERROR "Cannot find install manifest: \"/home/benoit/dev/Remmina/flatpak/_flatpak_build/install_manifest.txt\"")
endif( NOT EXISTS "/home/benoit/dev/Remmina/flatpak/_flatpak_build/install_manifest.txt" )

file( READ "/home/benoit/dev/Remmina/flatpak/_flatpak_build/install_manifest.txt" files)
string( REGEX REPLACE "\n" ";" files "${files}" )

foreach(file ${files})

    message(STATUS "Uninstalling \"$ENV{DESTDIR}${file}\"")
    if(EXISTS "$ENV{DESTDIR}${file}")

        exec_program( "/usr/bin/cmake"
            ARGS "-E remove \"$ENV{DESTDIR}${file}\""
            OUTPUT_VARIABLE rm_out
            RETURN_VALUE rm_retval
            )
        if( NOT "${rm_retval}" STREQUAL 0 )
            message( FATAL_ERROR "Problem when removing \"$ENV{DESTDIR}${file}\"" )
        endif( NOT "${rm_retval}" STREQUAL 0 )

    else(EXISTS "$ENV{DESTDIR}${file}")

        message( STATUS "File \"$ENV{DESTDIR}${file}\" does not exist." )

    endif(EXISTS "$ENV{DESTDIR}${file}")

    get_filename_component(FOLDER $ENV{DESTDIR}${file} DIRECTORY)
    set(FOLDERS ${FOLDERS} ${FOLDER})

endforeach(file)

# remove empty folders
message(STATUS "")
if(WITH_CLEANUP)
    message(STATUS "Removing empty folders")
else()
    message(STATUS "Empty folders that may be safely removed:")
endif()

list(APPEND FOLDERS
    "/CMake"
    "/app/lib/remmina"
    "/app/share/remmina"
    )

list(REMOVE_DUPLICATES FOLDERS)
foreach(dir ${FOLDERS})
    #   message(STATUS ${dir})
    delete_empty_folder(${dir})
    #   message(STATUS "")
endforeach(dir)

# delete_empty_folder("")
# delete_empty_folder("")
# delete_empty_folder("/CMake")
# delete_empty_folder("")
# delete_empty_folder("/app/lib/remmina")
# delete_empty_folder("/app/share/remmina")
message(STATUS "")
