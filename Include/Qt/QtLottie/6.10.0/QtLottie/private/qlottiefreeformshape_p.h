// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef LOTTIEFEEFORMSHAPE_P_H
#define LOTTIEFEEFORMSHAPE_P_H

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
#include <QJsonArray>
#include <QMap>
#include <QHash>
#include <QList>

#include <QtLottie/qlottieglobal.h>
#include <QtLottie/private/qlottieshape_p.h>
#include <QtLottie/private/qlottietrimpath_p.h>
#include <QtLottie/private/qlottierenderer_p.h>

QT_BEGIN_NAMESPACE

class QJsonObject;

class LOTTIE_EXPORT QLottieFreeFormShape : public QLottieShape
{
public:
    QLottieFreeFormShape();
    explicit QLottieFreeFormShape(const QLottieFreeFormShape &other);
    QLottieFreeFormShape(const QJsonObject &definition, const QVersionNumber &version,
                    QLottieBase *parent = nullptr);

    QLottieBase *clone() const override;

    void construct(const QJsonObject &definition, const QVersionNumber &version);

    void updateProperties(int frame) override;
    void render(QLottieRenderer &renderer) const override;

    bool acceptsTrim() const override;

protected:
    struct VertexInfo {
        QLottieProperty2D<QPointF> pos;
        QLottieProperty2D<QPointF> ci;
        QLottieProperty2D<QPointF> co;
    };

    void parseShapeKeyframes(QJsonObject &keyframes);
    void buildShape(const QJsonObject &keyframe);
    void buildShape(int frame);
    void parseEasedVertices(const QJsonObject &keyframe, int startFrame);

    QHash<int, QJsonObject> m_vertexMap;
    QList<VertexInfo> m_vertexList;
    QMap<int, bool> m_closedShape;

private:
    struct VertexBuildInfo
    {
        QJsonArray posKeyframes;
        QJsonArray ciKeyframes;
        QJsonArray coKeyframes;
    };

    void finalizeVertices();

    QMap<int, VertexBuildInfo*> m_vertexInfos;

    QJsonObject createKeyframe(QJsonArray startValue, QJsonArray endValue,
                               int startFrame, QJsonObject easingIn,
                               QJsonObject easingOut);
};

QT_END_NAMESPACE

#endif // LOTTIEFEEFORMSHAPE_P_H
