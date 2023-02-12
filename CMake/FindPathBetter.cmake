function(find_path_better OUTPUT_VAR PATTERN)
    cmake_parse_arguments(
            FIND_PATH_BETTER
            "" # list of names of the boolean arguments
            "" # list of names of mono-valued arguments
            "HINTS;PATHS;FILTER;NOT_FILTER" # list of names of multi-valued arguments
            ${ARGN})

    # The precedence of search paths resembles that of the find_paths command.
    # HINTS come before internal search locations. PATHS come after. Internally
    # this function only looks at CMAKE_PREFIX_PATH which variable is used by
    # projects to point to locations containing dependencies.
    set(SEARCH_PATHS "${FIND_PATH_BETTER_HINTS}")
    list(APPEND SEARCH_PATHS "${CMAKE_PREFIX_PATH}")
    list(APPEND SEARCH_PATHS "${FIND_PATH_BETTER_PATHS}")

    get_filename_component(FILENAME_OF_PATTERN "${PATTERN}" NAME)
    foreach (SEARCH_PATH ${SEARCH_PATHS})
        message(STATUS "Looking for ${PATTERN}")
        file(GLOB_RECURSE POSSIBLE_PATHS "${SEARCH_PATH}/*${FILENAME_OF_PATTERN}")
        list(FILTER POSSIBLE_PATHS INCLUDE REGEX ${PATTERN})
        if (FIND_PATH_BETTER_FILTER)
            foreach (FILTER ${FIND_PATH_BETTER_FILTER})
                list(FILTER POSSIBLE_PATHS INCLUDE REGEX ${FILTER})
            endforeach ()
        endif ()
        if (FIND_PATH_BETTER_NOT_FILTER)
            foreach (FILTER ${FIND_PATH_BETTER_NOT_FILTER})
                list(FILTER POSSIBLE_PATHS EXCLUDE REGEX ${FILTER})
            endforeach ()
        endif ()
        if (POSSIBLE_PATHS)
            list(GET POSSIBLE_PATHS 0 INTERMEDIATE_OUTPUT_VAR)
            string(REPLACE "${PATTERN}" "" INTERMEDIATE_OUTPUT_VAR
                    ${INTERMEDIATE_OUTPUT_VAR})
            get_filename_component(INTERMEDIATE_OUTPUT_VAR ${INTERMEDIATE_OUTPUT_VAR}
                    REALPATH)
        endif ()
        if (INTERMEDIATE_OUTPUT_VAR)
            set(${OUTPUT_VAR}
                    ${INTERMEDIATE_OUTPUT_VAR}
                    PARENT_SCOPE)
            break()
        else ()
            set(${OUTPUT_VAR}
                    "${OUTPUT_VAR}-NOTFOUND"
                    PARENT_SCOPE)
        endif ()
    endforeach ()
endfunction()

if (APPLE)
    # message(STATUS "Running under MacOS X") Watch out, for this check also is
    # TRUE under MacOS X because it falls under the category of Unix-like.
    set(INCLUDE_PATH_FILTER mac)
    set(LIB_PATH_FILTER mac)
    set(STATIC_LIB_POSTFIX ".a")
    set(IPP_PATHS "/opt/intel")
elseif (UNIX)
    # message(STATUS "Running under Unix or a Unix-like OS") Despite what you
    # might think given this name, the variable is also true for 64bit versions of
    # Windows.
elseif (WIN32)
    # message(STATUS "Running under Windows (either 32bit or 64bit)")
    set(INCLUDE_PATH_FILTER windows)
    set(LIB_PATH_FILTER windows)
    if (${CMAKE_SIZEOF_VOID_P} STREQUAL 4)
        set(LIB_PATH_NOT_FILTER intel64)
    else ()
        set(LIB_PATH_NOT_FILTER ia32)
    endif ()
    set(STATIC_LIB_POSTFIX ".lib")
    set(IPP_PATHS "C:/Program files (x86)/IntelSWTools"
            "C:/IntelSWTools")
endif ()
