// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#ifndef QQMLJSTRANSLATIONMISMATCHCHECK_P_H
#define QQMLJSTRANSLATIONMISMATCHCHECK_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#include "qqmlsa.h"

QT_BEGIN_NAMESPACE

class QQmlJSTranslationFunctionMismatchCheck : public QQmlSA::PropertyPass
{
public:
    using QQmlSA::PropertyPass::PropertyPass;

    void onCall(const QQmlSA::Element &element, const QString &propertyName,
                const QQmlSA::Element &readScope, QQmlSA::SourceLocation location) override;

private:
    enum TranslationType: quint8 { None, Normal, IdBased };
    TranslationType m_lastTranslationFunction = None;
};

QT_END_NAMESPACE
#endif // QQMLJSTRANSLATIONMISMATCHCHECK_P_H
