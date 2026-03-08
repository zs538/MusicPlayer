# - Find TagLib
# Finds the TagLib library
#
# This module defines:
#  TagLib_FOUND        - True if TagLib found.
#  TagLib_INCLUDE_DIRS - where to find taglib/*.h, etc.
#  TagLib_LIBRARIES    - List of libraries when using TagLib.
#  TagLib_VERSION      - TagLib version (if detectable)

find_path(TagLib_INCLUDE_DIR
  NAMES taglib/fileref.h
  PATHS
    /usr/include
    /usr/local/include
    /opt/local/include
    /sw/include
    ${CMAKE_PREFIX_PATH}/include
    /msys64/mingw64/include
    C:/msys64/mingw64/include
    DOC "TagLib include directory"
)

find_library(TagLib_LIBRARY
  NAMES tag taglib
  NAMES_PER_DIR
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
    ${CMAKE_PREFIX_PATH}/lib
    /msys64/mingw64/lib
    C:/msys64/mingw64/lib
    DOC "TagLib library"
)

# Handle the QUIETLY and REQUIRED arguments and set TagLib_FOUND to TRUE
# if all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TagLib
  REQUIRED_VARS TagLib_LIBRARY TagLib_INCLUDE_DIR
  VERSION_VAR TagLib_VERSION
)

if(TagLib_FOUND)
  set(TagLib_INCLUDE_DIRS ${TagLib_INCLUDE_DIR})
  set(TagLib_LIBRARIES ${TagLib_LIBRARY})

  # Add imported target
  if(NOT TARGET TagLib::TagLib)
    add_library(TagLib::TagLib UNKNOWN IMPORTED)
    set_target_properties(TagLib::TagLib PROPERTIES
      IMPORTED_LOCATION "${TagLib_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${TagLib_INCLUDE_DIR}"
    )
  endif()
endif()

mark_as_advanced(TagLib_INCLUDE_DIR TagLib_LIBRARY)
