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

class QQmlChangeSet;

class QQuickItemViewPrivate;

class Q_QUICK_PRIVATE_EXPORT QQuickItemView : public QQuickAbstractItemView
{
    Q_OBJECT

    Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QQmlComponent *delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)

    Q_PROPERTY(int cacheBuffer READ cacheBuffer WRITE setCacheBuffer NOTIFY cacheBufferChanged)
    Q_PROPERTY(int displayMarginBeginning READ displayMarginBeginning WRITE setDisplayMarginBeginning NOTIFY displayMarginBeginningChanged REVISION 2)
    Q_PROPERTY(int displayMarginEnd READ displayMarginEnd WRITE setDisplayMarginEnd NOTIFY displayMarginEndChanged REVISION 2)

    Q_PROPERTY(Qt::LayoutDirection layoutDirection READ layoutDirection WRITE setLayoutDirection NOTIFY layoutDirectionChanged)
    Q_PROPERTY(Qt::LayoutDirection effectiveLayoutDirection READ effectiveLayoutDirection NOTIFY effectiveLayoutDirectionChanged)
    Q_PROPERTY(VerticalLayoutDirection verticalLayoutDirection READ verticalLayoutDirection WRITE setVerticalLayoutDirection NOTIFY verticalLayoutDirectionChanged)

    Q_PROPERTY(QQmlComponent *header READ header WRITE setHeader NOTIFY headerChanged)
    Q_PROPERTY(QQuickItem *headerItem READ headerItem NOTIFY headerItemChanged)
    Q_PROPERTY(QQmlComponent *footer READ footer WRITE setFooter NOTIFY footerChanged)
    Q_PROPERTY(QQuickItem *footerItem READ footerItem NOTIFY footerItemChanged)

    Q_PROPERTY(QQmlComponent *highlight READ highlight WRITE setHighlight NOTIFY highlightChanged)
    Q_PROPERTY(QQuickItem *highlightItem READ highlightItem NOTIFY highlightItemChanged)
    Q_PROPERTY(bool highlightFollowsCurrentItem READ highlightFollowsCurrentItem WRITE setHighlightFollowsCurrentItem NOTIFY highlightFollowsCurrentItemChanged)
    Q_PROPERTY(HighlightRangeMode highlightRangeMode READ highlightRangeMode WRITE setHighlightRangeMode NOTIFY highlightRangeModeChanged)
    Q_PROPERTY(qreal preferredHighlightBegin READ preferredHighlightBegin WRITE setPreferredHighlightBegin NOTIFY preferredHighlightBeginChanged RESET resetPreferredHighlightBegin)
    Q_PROPERTY(qreal preferredHighlightEnd READ preferredHighlightEnd WRITE setPreferredHighlightEnd NOTIFY preferredHighlightEndChanged RESET resetPreferredHighlightEnd)
    Q_PROPERTY(int highlightMoveDuration READ highlightMoveDuration WRITE setHighlightMoveDuration NOTIFY highlightMoveDurationChanged)

public:
    QQuickItemView(QQuickFlickablePrivate &dd, QQuickItem *parent = 0);
    ~QQuickItemView();

    void setModel(const QVariant &);

    void setDelegate(QQmlComponent *);

    void setCacheBuffer(int);

    int displayMarginBeginning() const;
    void setDisplayMarginBeginning(int);

    int displayMarginEnd() const;
    void setDisplayMarginEnd(int);

    void setLayoutDirection(Qt::LayoutDirection);

    void setVerticalLayoutDirection(VerticalLayoutDirection layoutDirection);

    QQmlComponent *footer() const;
    void setFooter(QQmlComponent *);
    QQuickItem *footerItem() const;

    QQmlComponent *header() const;
    void setHeader(QQmlComponent *);
    QQuickItem *headerItem() const;

    QQmlComponent *highlight() const;
    void setHighlight(QQmlComponent *);

    QQuickItem *highlightItem() const;

    bool highlightFollowsCurrentItem() const;
    virtual void setHighlightFollowsCurrentItem(bool);

    enum HighlightRangeMode { NoHighlightRange, ApplyRange, StrictlyEnforceRange };
    Q_ENUM(HighlightRangeMode)
    HighlightRangeMode highlightRangeMode() const;
    void setHighlightRangeMode(HighlightRangeMode mode);

    qreal preferredHighlightBegin() const;
    void setPreferredHighlightBegin(qreal);
    void resetPreferredHighlightBegin();

    qreal preferredHighlightEnd() const;
    void setPreferredHighlightEnd(qreal);
    void resetPreferredHighlightEnd();

    int highlightMoveDuration() const;
    virtual void setHighlightMoveDuration(int);

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
    void modelChanged();
    void delegateChanged();

    void cacheBufferChanged();
    void displayMarginBeginningChanged();
    void displayMarginEndChanged();

    void layoutDirectionChanged();
    void effectiveLayoutDirectionChanged();
    void verticalLayoutDirectionChanged();

    void headerChanged();
    void footerChanged();
    void headerItemChanged();
    void footerItemChanged();

    void highlightChanged();
    void highlightItemChanged();
    void highlightFollowsCurrentItemChanged();
    void highlightRangeModeChanged();
    void preferredHighlightBeginChanged();
    void preferredHighlightEndChanged();
    void highlightMoveDurationChanged();

protected:
    void componentComplete() override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    qreal minYExtent() const override;
    qreal maxYExtent() const override;
    qreal minXExtent() const override;
    qreal maxXExtent() const override;

protected Q_SLOTS:
    void destroyRemoved();
    void createdItem(int index, QObject *item);
    void modelUpdated(const QQmlChangeSet &changeSet, bool reset);
    void animStopped();
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

