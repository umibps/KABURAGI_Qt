# Copyright (C) 2024 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

# Find "ModuleTools" dependencies, which are other ModuleTools packages.
set(Qt6LinguistTools_FOUND TRUE)

set(__qt_LinguistTools_tool_third_party_deps "")
_qt_internal_find_third_party_dependencies("LinguistTools" __qt_LinguistTools_tool_third_party_deps)

set(__qt_LinguistTools_tool_deps "")
_qt_internal_find_tool_dependencies("LinguistTools" __qt_LinguistTools_tool_deps)
