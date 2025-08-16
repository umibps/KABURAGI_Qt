// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QLOTTIEPRECOMPLAYER_P_H
#define QLOTTIEPRECOMPLAYER_P_H

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

#include <QtLottie/private/qlottielayer_p.h>

QT_BEGIN_NAMESPACE

class QLottieRenderer;

class LOTTIE_EXPORT QLottiePrecompLayer : public QLottieLayer
{
public:
    QLottiePrecompLayer() = default;
    explicit QLottiePrecompLayer(const QLottiePrecompLayer &other);
    QLottiePrecompLayer(const QJsonObject &definition, const QMap<QString, QJsonObject> &assets,
                        const QVersionNumber &version);

    QLottieBase *clone() const override;

    void render(QLottieRenderer &renderer) const override;
};

QT_END_NAMESPACE

#endif // QLOTTIEPRECOMPLAYER_P_H
