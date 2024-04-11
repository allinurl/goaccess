# - Try to find MaxMindDB headers and libraries
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  LIBMAXMINDDB_ROOT_DIR     Set this variable to the root installation of
#                            libmaxminddb if the module has problems finding the
#                            proper installation path.
#
# Variables defined by this module:
#
#  LIBMAXMINDDB_FOUND                   System has GeoIP libraries and headers
#  LIBMAXMINDDB_LIBRARY                 The GeoIP library
#  LIBMAXMINDDB_INCLUDE_DIR             The location of GeoIP headers

set(brew_search "/usr/local/Cellar/libmaxminddb")
if (APPLE AND (NOT DEFINED LIBMAXMINDDB_ROOT_DIR) AND (IS_DIRECTORY "${brew_search}"))
    file(GLOB children RELATIVE ${brew_search} ${brew_search}/*)
    set(dirlist "")
    foreach (child ${children})
        if (IS_DIRECTORY ${brew_search}/${child})
            LIST(APPEND include_search_list "${child}/include")
            LIST(APPEND lib_search_list "${child}/lib")
        endif ()
    endforeach ()
    set(brew_search "/usr/local/Cellar/libmaxminddb")
endif ()

find_path(LIBMAXMINDDB_INCLUDE_DIR
    NAMES maxminddb.h
    HINTS ${LIBMAXMINDDB_ROOT_DIR}/include ${include_search_list}
)

# We are just going to prefer static linking for this plugin.
set(libmaxminddb_names maxminddb libmaxminddb.a)

find_library(LIBMAXMINDDB_LIBRARY
    NAMES ${libmaxminddb_names}
    HINTS ${LIBMAXMINDDB_ROOT_DIR}/lib ${lib_search_list}
)

if (DEFINED LIBMAXMINDDB_INCLUDE_DIR AND DEFINED LIBMAXMINDDB_LIBRARY)
    set(LIBMAXMINDDB_FOUND 1)
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MaxMindDB DEFAULT_MSG
    LIBMAXMINDDB_LIBRARY
    LIBMAXMINDDB_INCLUDE_DIR
)

mark_as_advanced(
    LibMaxMindDB_ROOT_DIR
    LIBMAXMINDDB_LIBRARY
    LIBMAXMINDDB_INCLUDE_DIR
)

