// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QSVGCSSHANDLER_P_H
#define QSVGCSSHANDLER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtSvg/private/qtsvgglobal_p.h>
#include <QStringView>
#include <QList>
#include <QtGui/private/qcssparser_p.h>
#include <QtSvg/private/qsvgcssanimation_p.h>
#include <QtCore/private/qxmlstream_p.h>

QT_BEGIN_NAMESPACE

class QSvgStyleSelector;
class QSvgNode;
class QSvgCssHandler {
public:
    QSvgCssHandler();
    ~QSvgCssHandler();

    QSvgCssAnimation *createAnimation(const QString &name);
    void collectAnimations(const QCss::StyleSheet &sheet);

    void parseStyleSheet(const QStringView str);
    void parseCSStoXMLAttrs(const QList<QCss::Declaration> &declarations, QXmlStreamAttributes &attributes) const;
    void parseCSStoXMLAttrs(const QString &css, QXmlStreamAttributes &attributes) const;

    void styleLookup(QSvgNode *node, QXmlStreamAttributes &attributes) const;

private:
    void updateColorProperty(const QCss::Declaration &decl, QSvgAnimatedPropertyColor *property);
    void updateTransformProperty(const QCss::Declaration &decl, QSvgAnimatedPropertyTransform *property);

private:
    QHash<QString, QCss::AnimationRule> m_animations;
    QSvgStyleSelector *m_selector = nullptr;
};


QT_END_NAMESPACE

#endif // QSVGCSSHANDLER_P_H
