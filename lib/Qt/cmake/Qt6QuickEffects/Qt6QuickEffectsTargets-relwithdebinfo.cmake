#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Qt6::QuickEffects" for configuration "RelWithDebInfo"
set_property(TARGET Qt6::QuickEffects APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(Qt6::QuickEffects PROPERTIES
  IMPORTED_IMPLIB_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib/Qt6QuickEffects.lib"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/bin/Qt6QuickEffects.dll"
  )

list(APPEND _cmake_import_check_targets Qt6::QuickEffects )
list(APPEND _cmake_import_check_files_for_Qt6::QuickEffects "${_IMPORT_PREFIX}/lib/Qt6QuickEffects.lib" "${_IMPORT_PREFIX}/bin/Qt6QuickEffects.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
