// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QQUICKSIDEBAR_P_P_H
#define QQUICKSIDEBAR_P_P_H

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

#include <QtQuickTemplates2/private/qquickcontainer_p_p.h>
#include <QtCore/qstandardpaths.h>
#include <QtQuickTemplates2/private/qquickmenu_p.h>

QT_BEGIN_NAMESPACE

class QQuickContextMenu;

class Q_QUICKDIALOGS2QUICKIMPL_EXPORT QQuickSideBarPrivate : public QQuickContainerPrivate
{
public:
    Q_DECLARE_PUBLIC(QQuickSideBar)

    static QQuickSideBarPrivate *get(QQuickSideBar *sidebar)
    {
        return sidebar->d_func();
    }

    QQuickItem *createDelegateItem(QQmlComponent *component, const QVariantMap &initialProperties);
    void repopulate();
    void buttonClicked();

    QUrl dialogFolder() const;
    void setDialogFolder(const QUrl &folder);
    QString displayNameFromFolderPath(const QString &filePath);
    QQuickIcon folderIcon() const;
    QQuickIcon folderIcon(QStandardPaths::StandardLocation stdLocation) const;
    void folderChanged();

    void readSettings();
    void writeSettings() const;

    void addFavorite(const QUrl &favorite);
    void removeFavorite(const QUrl &favorite);
    bool showAddFavoriteDelegate() const;
    void setShowAddFavoriteDelegate(bool show);
    bool addFavoriteDelegateHovered() const;
    void setAddFavoriteDelegateHovered(bool hovered);
    QQuickIcon addFavoriteIcon() const;

    void initContextMenu();
    void handleContextMenuRequested(QPointF pos);
    void handleRemoveAction();

private:
    QQuickDialog *dialog = nullptr;
    QQmlComponent *buttonDelegate = nullptr;
    QQmlComponent *separatorDelegate = nullptr;
    QQmlComponent *addFavoriteDelegate = nullptr;
    QQuickContextMenu *contextMenu = nullptr;
    QQuickMenu *menu = nullptr;
    QQuickAction *removeAction = nullptr;
    QUrl urlToBeRemoved;
    QList<QStandardPaths::StandardLocation> folderPaths;
    QList<QUrl> favoritePaths;
    QUrl currentButtonClickedUrl;
    bool folderPathsValid = false;
    bool favoritePathsValid = false;
    bool repopulating = false;
    bool addFavoriteDelegateVisible = false;
    bool addFavoriteHovered = false;
    bool showSeparator = false;
};

QT_END_NAMESPACE

#endif // QQUICKSIDEBAR_P_P_H
