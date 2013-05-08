cmake_minimum_required (VERSION 2.6)
project (cpp-example CXX)

# The version number.
set (BSS_VERSION_MAJOR 1)
set (BSS_VERSION_MINOR 0)
set (BSS_VERSION_REVISION 0)

set(CMAKE_CXX_FLAGS "-O3 -std=c++0x -Wall -Wextra -Wno-sign-compare")

#Set the owl include path
FIND_PATH(OWL_INC_DIR NAMES owl PATHS /usr/include /usr/local/include)
IF(NOT OWL_INC_DIR)
  MESSAGE(FATAL_ERROR "Unable to find libowl installation!")
ELSE(NOT OWL_INC_DIR)
  #Truncate the search path
  STRING(REGEX REPLACE "/owl" "" OWL_INC_DIR ${OWL_INC_DIR})
ENDIF(NOT OWL_INC_DIR)

include_directories ("${OWL_INC_DIR}")

add_subdirectory (src bin)

#Set the correct library directory suffix
if(NOT DEFINED LIB_SUFFIX)
  get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
  if (${LIB64} STREQUAL "TRUE")
    set(LIBSUFFIX 64)
  else()
    set(LIBSUFFIX "")
  endif()
  set(LIB_SUFFIX ${LIBSUFFIX} CACHE STRING "lib directory suffix")
endif()

