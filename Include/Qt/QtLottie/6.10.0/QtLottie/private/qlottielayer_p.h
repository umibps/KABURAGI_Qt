// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QLOTTIELAYER_P_H
#define QLOTTIELAYER_P_H

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

#include <QtLottie/private/qlottiebase_p.h>
#include <QtCore/QSize>

QT_BEGIN_NAMESPACE

class QLottieRenderer;

class LOTTIE_EXPORT QLottieLayer : public QLottieBase
{
public:
    enum MatteClipMode {NoClip, Alpha, InvertedAlpha, Luminence, InvertedLuminence};

    QLottieLayer() = default;
   explicit  QLottieLayer (const QLottieLayer &other);
    ~QLottieLayer() override;

    QLottieBase *clone() const override;

    static QLottieLayer *construct(QJsonObject definition, const QMap<QString, QJsonObject> &assets,
                                   const QVersionNumber &version);
    static int constructLayers(QJsonArray jsonLayers, QLottieBase *parent,
                               const QMap<QString, QJsonObject> &assets,
                               const QVersionNumber &version);

    bool active(int frame) const override;

    void parse(const QJsonObject &definition) override;

    void updateProperties(int frame) override;
    void render(QLottieRenderer &renderer) const override;

    QLottieBase *findChild(const QString &childName) override;

    bool isClippedLayer() const;
    bool isMaskLayer() const;
    MatteClipMode clipMode() const;

    int layerId() const;
    QLottieBasicTransform *transform() const;
    void applyLinkedTransforms(QLottieRenderer &renderer) const;
    QSize size() const;

protected:
    void renderEffects(QLottieRenderer &renderer) const;

    virtual QLottieLayer *resolveLinkedLayer();
    virtual QLottieLayer *linkedLayer() const;

    int m_layerIndex = 0;
    int m_startFrame = 0;
    int m_endFrame = 0;
    qreal m_startTime = 0;
    int m_blendMode = 0;
    bool m_3dLayer = false;
    QLottieBase *m_effects = nullptr;
    qreal m_stretch = 0;
    QLottieBasicTransform *m_layerTransform = nullptr;

    int m_parentLayer = 0;
    int m_td = 0;
    MatteClipMode m_clipMode = NoClip;

    bool m_isActive = true;
    QSize m_size;

private:
    void parseEffects(const QJsonArray &definition, QLottieBase *effectRoot = nullptr);

    mutable bool m_applyingLinkedTransforms = false;
    QLottieLayer *m_linkedLayer = nullptr;
};

QT_END_NAMESPACE

#endif // QLOTTIELAYER_P_H
