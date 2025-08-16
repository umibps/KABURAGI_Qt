# Copyright (C) 2024 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

if(NOT QT_NO_CREATE_TARGETS)
    set(_ProtobufQtCoreTypes_proto_include_dirs "include/QtProtobufQtCoreTypes")
    foreach(proto_include_dir IN LISTS _ProtobufQtCoreTypes_proto_include_dirs)
        set_property(TARGET ${QT_CMAKE_EXPORT_NAMESPACE}::ProtobufQtCoreTypes APPEND PROPERTY
            QT_PROTO_INCLUDES "${QT6_INSTALL_PREFIX}/${proto_include_dir}")
        if(CMAKE_STAGING_PREFIX)
            set_property(TARGET ${QT_CMAKE_EXPORT_NAMESPACE}::ProtobufQtCoreTypes APPEND PROPERTY
                QT_PROTO_INCLUDES "${CMAKE_STAGING_PREFIX}/${proto_include_dir}")
        endif()
    endforeach()
    unset(_ProtobufQtCoreTypes_proto_include_dirs)
endif()
