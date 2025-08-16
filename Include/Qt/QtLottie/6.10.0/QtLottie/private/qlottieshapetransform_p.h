// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QLOTTIESHAPETRANSFORM_P_H
#define QLOTTIESHAPETRANSFORM_P_H

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

#include <QPointF>

#include <QtLottie/private/qlottieshape_p.h>
#include <QtLottie/private/qlottiebasictransform_p.h>
#include <QtLottie/private/qlottieproperty_p.h>

QT_BEGIN_NAMESPACE

class QJsonObject;

class LOTTIE_EXPORT QLottieShapeTransform :  public QLottieBasicTransform
{
public:
    explicit QLottieShapeTransform(const QLottieShapeTransform &other);
    QLottieShapeTransform(const QJsonObject &definition, const QVersionNumber &version, QLottieBase *parent);

    QLottieBase *clone() const override;

    void construct(const QJsonObject &definition, const QVersionNumber &version);

    void updateProperties(int frame) override;
    void render(QLottieRenderer &renderer) const override;

    qreal skew() const;
    qreal skewAxis() const;
    qreal shearX() const;
    qreal shearY() const;
    qreal shearAngle() const;

protected:
    QLottieProperty<qreal> m_skew;
    QLottieProperty<qreal> m_skewAxis;
    qreal m_shearX = 0;
    qreal m_shearY = 0;
    qreal m_shearAngle = 0;
};

QT_END_NAMESPACE

#endif // QLOTTIESHAPETRANSFORM_P_H
