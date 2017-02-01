/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKITEMVIEW_P_H
#define QQUICKITEMVIEW_P_H

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

#include <QtQuick/private/qtquickglobal_p.h>

QT_REQUIRE_CONFIG(quick_itemview);

#include "qquickabstractitemview_p.h"
#include <qpointer.h>
#include <QtCore/QLoggingCategory>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcItemViewDelegateLifecycle)

class QQuickItemViewPrivate;

class Q_QUICK_PRIVATE_EXPORT QQuickItemView : public QQuickAbstractItemView
{
    Q_OBJECT

    Q_PROPERTY(int displayMarginBeginning READ displayMarginBeginning WRITE setDisplayMarginBeginning NOTIFY displayMarginBeginningChanged REVISION 2)
    Q_PROPERTY(int displayMarginEnd READ displayMarginEnd WRITE setDisplayMarginEnd NOTIFY displayMarginEndChanged REVISION 2)

    Q_PROPERTY(QQmlComponent *header READ header WRITE setHeader NOTIFY headerChanged)
    Q_PROPERTY(QQuickItem *headerItem READ headerItem NOTIFY headerItemChanged)
    Q_PROPERTY(QQmlComponent *footer READ footer WRITE setFooter NOTIFY footerChanged)
    Q_PROPERTY(QQuickItem *footerItem READ footerItem NOTIFY footerItemChanged)

public:
    QQuickItemView(QQuickFlickablePrivate &dd, QQuickItem *parent = 0);
    ~QQuickItemView();

    int displayMarginBeginning() const;
    void setDisplayMarginBeginning(int);

    int displayMarginEnd() const;
    void setDisplayMarginEnd(int);

    QQmlComponent *footer() const;
    void setFooter(QQmlComponent *);
    QQuickItem *footerItem() const;

    QQmlComponent *header() const;
    void setHeader(QQmlComponent *);
    QQuickItem *headerItem() const;

    enum PositionMode { Beginning, Center, End, Visible, Contain, SnapPosition };
    Q_ENUM(PositionMode)

    Q_INVOKABLE void positionViewAtIndex(int index, int mode);
    Q_INVOKABLE int indexAt(qreal x, qreal y) const;
    Q_INVOKABLE QQuickItem *itemAt(qreal x, qreal y) const;
    Q_INVOKABLE void positionViewAtBeginning();
    Q_INVOKABLE void positionViewAtEnd();

    void setContentX(qreal pos) override;
    void setContentY(qreal pos) override;
    qreal originX() const override;
    qreal originY() const override;

Q_SIGNALS:
    void displayMarginBeginningChanged();
    void displayMarginEndChanged();

    void headerChanged();
    void footerChanged();
    void headerItemChanged();
    void footerItemChanged();

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    qreal minYExtent() const override;
    qreal maxYExtent() const override;
    qreal minXExtent() const override;
    qreal maxXExtent() const override;

protected Q_SLOTS:
    void trackedPositionChanged() override;

private:
    Q_DECLARE_PRIVATE(QQuickItemView)
};


class Q_QUICK_PRIVATE_EXPORT QQuickItemViewAttached : public QQuickAbstractItemViewAttached
{
    Q_OBJECT

public:
    QQuickItemViewAttached(QObject *parent)
        : QQuickAbstractItemViewAttached(parent) {}
    ~QQuickItemViewAttached() {}
};


QT_END_NAMESPACE

#endif // QQUICKITEMVIEW_P_H

