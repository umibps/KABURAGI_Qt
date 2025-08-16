#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Qt6::QmlAssetDownloader" for configuration "RelWithDebInfo"
set_property(TARGET Qt6::QmlAssetDownloader APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(Qt6::QmlAssetDownloader PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "CXX"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib/Qt6QmlAssetDownloader.lib"
  )

list(APPEND _cmake_import_check_targets Qt6::QmlAssetDownloader )
list(APPEND _cmake_import_check_files_for_Qt6::QmlAssetDownloader "${_IMPORT_PREFIX}/lib/Qt6QmlAssetDownloader.lib" )

# Import target "Qt6::QmlAssetDownloader_resources_1" for configuration "RelWithDebInfo"
set_property(TARGET Qt6::QmlAssetDownloader_resources_1 APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(Qt6::QmlAssetDownloader_resources_1 PROPERTIES
  IMPORTED_COMMON_LANGUAGE_RUNTIME_RELWITHDEBINFO ""
  IMPORTED_OBJECTS_RELWITHDEBINFO "${_IMPORT_PREFIX}/qml/Assets/Downloader/objects-RelWithDebInfo/QmlAssetDownloader_resources_1/.qt/rcc/qrc_qmake_Assets_Downloader_init.cpp.obj"
  )

list(APPEND _cmake_import_check_targets Qt6::QmlAssetDownloader_resources_1 )
list(APPEND _cmake_import_check_files_for_Qt6::QmlAssetDownloader_resources_1 "${_IMPORT_PREFIX}/qml/Assets/Downloader/objects-RelWithDebInfo/QmlAssetDownloader_resources_1/.qt/rcc/qrc_qmake_Assets_Downloader_init.cpp.obj" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
