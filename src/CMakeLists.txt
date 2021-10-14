set(LIBRARY_NAME "goaccess")

set(Header_Files
        "base64.h"
        "bitmap.h"
        "browsers.h"
        "color.h"
        "commons.h"
        "csv.h"
        "error.h"
        "gdashboard.h"
        "gdns.h"
        "geoip1.h"
        "gholder.h"
        "gkhash.h"
        "gmenu.h"
        "goaccess.h"
        "gslist.h"
        "gstorage.h"
        "gwsocket.h"
        "json.h"
        "khash.h"
        "labels.h"
        "opesys.h"
        "options.h"
        "output.h"
        "parser.h"
        "pdjson.h"
        "persistence.h"
        "settings.h"
        "sha1.h"
        "sort.h"
        "tpl.h"
        "ui.h"
        "util.h"
        "websocket.h"
        "xmalloc.h"
)
source_group("Header Files" FILES "${Header_Files}")

set(Source_Files
        "base64.c"
        "bin2c.c"
        "bitmap.c"
        "browsers.c"
        "color.c"
        "commons.c"
        "csv.c"
        "error.c"
        "gdashboard.c"
        "gdns.c"
        "geoip1.c"
        "geoip2.c"
        "gholder.c"
        "gkhash.c"
        "gmenu.c"
        "goaccess.c"
        "gslist.c"
        "gstorage.c"
        "gwsocket.c"
        "json.c"
        "opesys.c"
        "options.c"
        "output.c"
        "parser.c"
        "pdjson.c"
        "persistence.c"
        "settings.c"
        "sha1.c"
        "sort.c"
        "tpl.c"
        "ui.c"
        "util.c"
        "websocket.c"
        "xmalloc.c"
)
source_group("Source Files" FILES "${Source_Files}")

add_library("${LIBRARY_NAME}" "${LIBRARY_TYPE_FLAG}" "${Header_Files}" "${Source_Files}")
target_include_directories(
        "${LIBRARY_NAME}"
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:include>"
)

include(FindIntl)
include(FindCurses)
find_package(Curses)
if (CURSES_HAVE_NCURSES_H)
    target_compile_definitions("${LIBRARY_NAME}" PUBLIC HAVE_NCURSES_H=1)
endif ()
if (CURSES_HAVE_CURSES_H)
    target_compile_definitions("${LIBRARY_NAME}" PUBLIC HAVE_CURSES_H=1)
endif ()
if (CURSES_HAVE_NCURSES_NCURSES_H)
    target_compile_definitions("${LIBRARY_NAME}" PUBLIC HAVE_NCURSES_NCURSES_H=1)
endif ()
include(FindThreads)
if (CMAKE_USE_PTHREADS_INIT)
    target_compile_definitions("${LIBRARY_NAME}" PUBLIC HAVE_LIBPTHREAD=1)
else ()
    message(FATAL_ERROR "${PROJECT_NAME} requires pthreads")
endif ()
include(../cmake/FindGeoIP.cmake)
if (GeoIP_FOUND)
    target_compile_definitions("${LIBRARY_NAME}" PUBLIC HAVE_LIBGEOIP=1)
else ()
    message(FATAL_ERROR "${PROJECT_NAME} requires GeoIP")
endif ()
include(../cmake/FindMaxMindDB.cmake)
if (LIBMAXMINDDB_FOUND)
    target_compile_definitions("${LIBRARY_NAME}" PUBLIC HAVE_LIBMAXMINDDB=1)
else ()
    message(FATAL_ERROR "${PROJECT_NAME} requires MaxMindDB")
endif ()
include(GNUInstallDirs)
if (DEFINED CMAKE_INSTALL_FULL_LOCALEDIR)
    target_compile_definitions("${LIBRARY_NAME}" PUBLIC LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")
endif ()

target_link_libraries(
        "${LIBRARY_NAME}"
        PRIVATE
        "Intl::Intl"
        "${CURSES_LIBRARIES}"
        "Threads::Threads"
        "${GeoIP_LIBRARIES}"
        "${LIBMAXMINDDB_LIBRARY}"
)

set_target_properties(
        "${LIBRARY_NAME}"
        PROPERTIES
        LINKER_LANGUAGE
        C
)

# install rules
include(GenerateExportHeader)
set(_export_file "${CMAKE_CURRENT_SOURCE_DIR}/${LIBRARY_NAME}_export.h")
generate_export_header("${LIBRARY_NAME}" EXPORT_FILE_NAME "${_export_file}")

# setup the version numbering
set_property(TARGET "${LIBRARY_NAME}" PROPERTY VERSION "1.0.0")
set_property(TARGET "${LIBRARY_NAME}" PROPERTY SOVERSION "1")

set(installable_libs "${LIBRARY_NAME}" "${PROJECT_NAME}_compiler_flags")
install(FILES "${Header_Files}" DESTINATION "include")

if (TARGET "${DEPENDANT_LIBRARY}")
    list(APPEND installable_libs "${DEPENDANT_LIBRARY}")
endif ()
install(TARGETS ${installable_libs}
        DESTINATION "lib"
        EXPORT "${EXEC_NAME}Targets")
