# - Find GeoIP
# Find the GeoIP libraries
#
#  This module defines the following variables:
#     GeoIP_FOUND       - True if GeoIP_INCLUDE_DIR & GeoIP_LIBRARY are found
#     GeoIP_LIBRARIES   - Set when GeoIP_LIBRARY is found
#     GeoIP_INCLUDE_DIRS - Set when GeoIP_INCLUDE_DIR is found
#
#     GeoIP_INCLUDE_DIR - where to find GeoIP.h and GeoIPCity.h.
#     GeoIP_LIBRARY     - the GeoIP library
#

#=============================================================================
# Copyright 2012 piernov <piernov@piernov.org>
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

find_path(GeoIP_INCLUDE_DIR NAMES GeoIP.h GeoIPCity.h)

find_library(GeoIP_LIBRARY NAMES GeoIP)

# handle the QUIETLY and REQUIRED arguments and set GeoIP_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GeoIP DEFAULT_MSG GeoIP_LIBRARY GeoIP_INCLUDE_DIR)

set(GeoIP_FOUND ${GEOIP_FOUND})

if(GeoIP_FOUND)
  set(GeoIP_LIBRARIES ${GeoIP_LIBRARY})
  set(GeoIP_INCLUDE_DIRS ${GeoIP_INCLUDE_DIR})
endif()

mark_as_advanced(GeoIP_INCLUDE_DIR GeoIP_LIBRARY)
