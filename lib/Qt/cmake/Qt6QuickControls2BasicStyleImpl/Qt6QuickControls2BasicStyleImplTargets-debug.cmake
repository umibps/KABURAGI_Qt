#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Qt6::QuickControls2BasicStyleImpl" for configuration "Debug"
set_property(TARGET Qt6::QuickControls2BasicStyleImpl APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(Qt6::QuickControls2BasicStyleImpl PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/lib/Qt6QuickControls2BasicStyleImpld.lib"
  IMPORTED_LINK_DEPENDENT_LIBRARIES_DEBUG "Qt6::Core;Qt6::Gui;Qt6::Qml;Qt6::QuickControls2Impl;Qt6::Quick;Qt6::QuickTemplates2"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/Qt6QuickControls2BasicStyleImpld.dll"
  )

list(APPEND _cmake_import_check_targets Qt6::QuickControls2BasicStyleImpl )
list(APPEND _cmake_import_check_files_for_Qt6::QuickControls2BasicStyleImpl "${_IMPORT_PREFIX}/lib/Qt6QuickControls2BasicStyleImpld.lib" "${_IMPORT_PREFIX}/bin/Qt6QuickControls2BasicStyleImpld.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
