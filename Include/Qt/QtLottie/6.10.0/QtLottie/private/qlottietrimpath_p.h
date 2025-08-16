// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QLOTTIETRIMPATH_P_H
#define QLOTTIETRIMPATH_P_H

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

#include <QPainterPath>
#include <QJsonObject>

#include <QtLottie/private/qlottieproperty_p.h>
#include <QtLottie/private/qlottiegroup_p.h>

QT_BEGIN_NAMESPACE

class LOTTIE_EXPORT QLottieTrimPath : public QLottieShape
{
public:
    QLottieTrimPath();
    QLottieTrimPath(const QJsonObject &definition, const QVersionNumber &version,
               QLottieBase *parent = nullptr);
    explicit QLottieTrimPath(const QLottieTrimPath &other);

    void inherit(const QLottieTrimPath &other);

    QLottieBase *clone() const override;

    void construct(const QJsonObject &definition, const QVersionNumber &version);

    void updateProperties(int frame) override;
    void render(QLottieRenderer &renderer) const override;

    bool acceptsTrim() const override;
    void applyTrim(const QLottieTrimPath  &trimmer) override;

    qreal start() const;
    qreal end() const;
    qreal offset() const;
    bool isParallel() const;

    QPainterPath trim(const QPainterPath &path) const;

protected:
    QLottieProperty<qreal> m_start;
    QLottieProperty<qreal> m_end;
    QLottieProperty<qreal> m_offset;
    bool m_isParallel = false;
};

QT_END_NAMESPACE

#endif // QLOTTIETRIMPATH_P_H
