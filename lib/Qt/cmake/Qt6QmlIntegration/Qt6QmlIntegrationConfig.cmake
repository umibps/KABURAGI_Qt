# Copyright (C) 2024 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was QtModuleConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

cmake_minimum_required(VERSION 3.16...3.21)

include(CMakeFindDependencyMacro)

# Extra cmake code begin

# Extra cmake code end

# Find required dependencies, if any.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/Qt6QmlIntegrationDependencies.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/Qt6QmlIntegrationDependencies.cmake")
    _qt_internal_suggest_dependency_debugging(QmlIntegration
        __qt_QmlIntegration_pkg ${CMAKE_FIND_PACKAGE_NAME}_NOT_FOUND_MESSAGE)
endif()

# If *ConfigDependencies.cmake exists, the variable value will be defined there.
# Don't override it in that case.
if(NOT DEFINED "Qt6QmlIntegration_FOUND")
    set("Qt6QmlIntegration_FOUND" TRUE)
endif()

if (NOT QT_NO_CREATE_TARGETS AND Qt6QmlIntegration_FOUND)
    include("${CMAKE_CURRENT_LIST_DIR}/Qt6QmlIntegrationTargets.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/Qt6QmlIntegrationAdditionalTargetInfo.cmake"
        OPTIONAL)
    include("${CMAKE_CURRENT_LIST_DIR}/Qt6QmlIntegrationExtraProperties.cmake"
        OPTIONAL)
endif()

# Find the private module counterpart.
set(__qt_QmlIntegration_always_load_private_module OFF)
include("${CMAKE_CURRENT_LIST_DIR}/Qt6QmlIntegration-build.cmake" OPTIONAL)
if (Qt6QmlIntegration_FOUND
        AND NOT Qt6QmlIntegrationPrivate_FOUND
        AND NOT TRUE
        AND (
            __qt_QmlIntegration_always_load_private_module
            OR DEFINED QT_REPO_MODULE_VERSION
            OR QT_FIND_PRIVATE_MODULES
        )
    )
    if("${_qt_cmake_dir}" STREQUAL "")
        set(_qt_cmake_dir "${QT_TOOLCHAIN_RELOCATABLE_CMAKE_DIR}")
    endif()
    set(__qt_use_no_default_path_for_qt_packages "NO_DEFAULT_PATH")
    if(QT_DISABLE_NO_DEFAULT_PATH_IN_QT_PACKAGES)
        set(__qt_use_no_default_path_for_qt_packages "")
    endif()
    find_package(Qt6QmlIntegrationPrivate "6.10.0" EXACT
        QUIET
        CONFIG
        PATHS
            ${QT_BUILD_CMAKE_PREFIX_PATH}
            "${CMAKE_CURRENT_LIST_DIR}/.."
            "${_qt_cmake_dir}"
            ${_qt_additional_packages_prefix_paths}
        ${__qt_use_no_default_path_for_qt_packages}
    )

    if(NOT Qt6QmlIntegrationPrivate_FOUND)
        get_property(Qt6QmlIntegrationPrivate_warning_shown GLOBAL PROPERTY
            Qt6QmlIntegrationPrivate_warning_shown
        )
        if(NOT Qt6QmlIntegrationPrivate_warning_shown)
            message(VERBOSE
                "The private module package 'Qt6QmlIntegrationPrivate' "
                "could not be found. It possibly needs to be installed separately with your "
                "package manager."
            )
            set_property(GLOBAL PROPERTY
                Qt6QmlIntegrationPrivate_warning_shown ON
            )
        endif()
    endif()
endif()
unset(__qt_QmlIntegration_always_load_private_module)

if (NOT QT_NO_CREATE_TARGETS AND Qt6QmlIntegration_FOUND)
    # DEPRECATED
    # Provide old style variables for includes, compile definitions, etc.
    # These variables are deprecated and only provided on a best-effort basis to facilitate porting.
    # Consider using target_link_libraries(app PRIVATE Qt6QmlIntegration) instead.
    set(Qt6QmlIntegration_LIBRARIES "Qt6::QmlIntegration")

    get_target_property(_Qt6QmlIntegration_OWN_INCLUDE_DIRS
                        Qt6::QmlIntegration INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT _Qt6QmlIntegration_OWN_INCLUDE_DIRS)
        set(_Qt6QmlIntegration_OWN_INCLUDE_DIRS "")
    endif()

    if(TARGET Qt6::QmlIntegrationPrivate)
        get_target_property(_Qt6QmlIntegration_OWN_PRIVATE_INCLUDE_DIRS
                            Qt6::QmlIntegrationPrivate INTERFACE_INCLUDE_DIRECTORIES)
        if(NOT _Qt6QmlIntegration_OWN_PRIVATE_INCLUDE_DIRS)
            set(_Qt6QmlIntegration_OWN_PRIVATE_INCLUDE_DIRS "")
        endif()
    endif()

    get_target_property(Qt6QmlIntegration_DEFINITIONS
                        Qt6::QmlIntegration INTERFACE_COMPILE_DEFINITIONS)
    if(NOT Qt6QmlIntegration_DEFINITIONS)
        set(Qt6QmlIntegration_DEFINITIONS "")
    else()
        set(updated_defs "")
        foreach(def IN LISTS  Qt6QmlIntegration_DEFINITIONS)
             if(def MATCHES "^[A-Za-z_]")
                 list(APPEND updated_defs "-D${def}")
             else()
                 list(APPEND updated_defs "${def}")
             endif()
        endforeach()
        set(Qt6QmlIntegration_DEFINITIONS "${updated_defs}")
        unset(updated_defs)
    endif()

    get_target_property(Qt6QmlIntegration_COMPILE_DEFINITIONS
                        Qt6::QmlIntegration INTERFACE_COMPILE_DEFINITIONS)
    if(NOT Qt6QmlIntegration_COMPILE_DEFINITIONS)
        set(Qt6QmlIntegration_COMPILE_DEFINITIONS "")
    endif()

    set(Qt6QmlIntegration_INCLUDE_DIRS
        ${_Qt6QmlIntegration_OWN_INCLUDE_DIRS})

    set(Qt6QmlIntegration_PRIVATE_INCLUDE_DIRS
        ${_Qt6QmlIntegration_OWN_PRIVATE_INCLUDE_DIRS})

    foreach(_module_dep ${_Qt6QmlIntegration_MODULE_DEPENDENCIES})
        if(_module_dep MATCHES ".+Private$")
            set(_private_suffix "Private")
        else()
            set(_private_suffix "")
        endif()
        list(APPEND Qt6QmlIntegration${_private_suffix}_INCLUDE_DIRS
             ${Qt6${_module_dep}_INCLUDE_DIRS})
        list(APPEND Qt6QmlIntegration${_private_suffix}_PRIVATE_INCLUDE_DIRS
             ${Qt6${_module_dep}_PRIVATE_INCLUDE_DIRS})
        if(_private_suffix)
            list(APPEND Qt6QmlIntegration_PRIVATE_INCLUDE_DIRS
                ${Qt6${_module_dep}_PRIVATE_INCLUDE_DIRS})
        endif()
        list(APPEND Qt6QmlIntegration${_private_suffix}_DEFINITIONS
             ${Qt6${_module_dep}_DEFINITIONS})
        list(APPEND Qt6QmlIntegration${_private_suffix}_COMPILE_DEFINITIONS
             ${Qt6${_module_dep}_COMPILE_DEFINITIONS})
    endforeach()
    unset(_private_suffix)

    list(REMOVE_DUPLICATES Qt6QmlIntegration_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES Qt6QmlIntegration_PRIVATE_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES Qt6QmlIntegration_DEFINITIONS)
    list(REMOVE_DUPLICATES Qt6QmlIntegration_COMPILE_DEFINITIONS)
endif()

if (TARGET Qt6::QmlIntegration)
    qt_make_features_available(Qt6::QmlIntegration)

    foreach(extra_cmake_include )
        include("${CMAKE_CURRENT_LIST_DIR}/${extra_cmake_include}")
    endforeach()

    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/Qt6QmlIntegrationPlugins.cmake")
        include("${CMAKE_CURRENT_LIST_DIR}/Qt6QmlIntegrationPlugins.cmake")
    endif()

    if(NOT "QmlIntegration" IN_LIST QT_ALL_MODULES_FOUND_VIA_FIND_PACKAGE)
        list(APPEND QT_ALL_MODULES_FOUND_VIA_FIND_PACKAGE "QmlIntegration")
        list(APPEND QT_ALL_MODULES_VERSIONED_FOUND_VIA_FIND_PACKAGE
            "Qt6::QmlIntegration")
    endif()

    get_target_property(_qt_module_target_type "Qt6::QmlIntegration" TYPE)
    if(NOT _qt_module_target_type STREQUAL "INTERFACE_LIBRARY")
        get_target_property(_qt_module_plugin_types
                            Qt6::QmlIntegration MODULE_PLUGIN_TYPES)
        if(_qt_module_plugin_types)
            foreach(_qt_module_plugin_type IN LISTS _qt_module_plugin_types)
                if(NOT "${_qt_module_plugin_type}"
                    IN_LIST QT_ALL_PLUGIN_TYPES_FOUND_VIA_FIND_PACKAGE)
                    list(APPEND QT_ALL_PLUGIN_TYPES_FOUND_VIA_FIND_PACKAGE
                    "${_qt_module_plugin_type}")
                endif()
            endforeach()
            unset(_qt_module_plugin_type)
        endif()
    endif()

    # Load Module's BuildInternals should any exist
    if (Qt6BuildInternals_DIR AND
        EXISTS "${CMAKE_CURRENT_LIST_DIR}/Qt6QmlIntegrationBuildInternals.cmake")
        include("${CMAKE_CURRENT_LIST_DIR}/Qt6QmlIntegrationBuildInternals.cmake")
    endif()

    if(NOT QT_NO_CREATE_VERSIONLESS_TARGETS)
        if(CMAKE_VERSION VERSION_LESS 3.18 OR QT_USE_OLD_VERSION_LESS_TARGETS)
            include("${CMAKE_CURRENT_LIST_DIR}/Qt6QmlIntegrationVersionlessTargets.cmake")
        else()
            include("${CMAKE_CURRENT_LIST_DIR}/Qt6QmlIntegrationVersionlessAliasTargets.cmake")
        endif()
    endif()
else()

    set(Qt6QmlIntegration_FOUND FALSE)
    if(NOT DEFINED Qt6QmlIntegration_NOT_FOUND_MESSAGE)
        set(Qt6QmlIntegration_NOT_FOUND_MESSAGE
            "Target \"Qt6::QmlIntegration\" was not found.")

        if(QT_NO_CREATE_TARGETS)
            string(APPEND Qt6QmlIntegration_NOT_FOUND_MESSAGE
                "Possibly due to QT_NO_CREATE_TARGETS being set to TRUE and thus "
                "${CMAKE_CURRENT_LIST_DIR}/Qt6QmlIntegrationTargets.cmake was not "
                "included to define the target.")
        endif()
    endif()
endif()
