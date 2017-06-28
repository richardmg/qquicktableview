/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef QQUICKABSTRACTITEMVIEW_P_P_H
#define QQUICKABSTRACTITEMVIEW_P_P_H

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
#include "qquickitemviewtransition_p.h"
#include "qquickflickable_p_p.h"

#include <QtQml/private/qqmlchangeset_p.h>
#include <QtQml/private/qqmlobjectmodel_p.h>

QT_BEGIN_NAMESPACE

class Q_AUTOTEST_EXPORT FxViewItem
{
public:
    FxViewItem(QQuickItem *, QQuickAbstractItemView *, bool own, QQuickItemViewAttached *attached);
    virtual ~FxViewItem();

    qreal itemX() const;
    qreal itemY() const;
    inline qreal itemWidth() const { return item ? item->width() : 0; }
    inline qreal itemHeight() const { return item ? item->height() : 0; }

    void moveTo(const QPointF &pos, bool immediate);
    void setVisible(bool visible);
    void trackGeometry(bool track);

    QQuickItemViewTransitioner::TransitionType scheduledTransitionType() const;
    bool transitionScheduledOrRunning() const;
    bool transitionRunning() const;
    bool isPendingRemoval() const;

    void transitionNextReposition(QQuickItemViewTransitioner *transitioner, QQuickItemViewTransitioner::TransitionType type, bool asTarget);
    bool prepareTransition(QQuickItemViewTransitioner *transitioner, const QRectF &viewBounds);
    void startTransition(QQuickItemViewTransitioner *transitioner);

    virtual bool contains(qreal x, qreal y) const = 0;

    QPointer<QQuickItem> item;
    QQuickAbstractItemView *view;
    QQuickItemViewTransitionableItem *transitionableItem;
    QQuickItemViewAttached *attached;
    int index;
    bool ownItem;
    bool releaseAfterTransition;
    bool trackGeom;
};

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, FxViewItem *item);
#endif

class QQuickItemViewChangeSet
{
public:
    QQuickItemViewChangeSet();

    bool hasPendingChanges() const;
    void prepare(int currentIndex, int count);
    void reset();

    void applyChanges(const QQmlChangeSet &changeSet);

    void applyBufferedChanges(const QQuickItemViewChangeSet &other);

    int itemCount;
    int newCurrentIndex;
    QQmlChangeSet pendingChanges;
    QHash<QQmlChangeSet::MoveKey, FxViewItem *> removedItems;

    bool active : 1;
    bool currentChanged : 1;
    bool currentRemoved : 1;
};

class Q_AUTOTEST_EXPORT QQuickAbstractItemViewPrivate : public QQuickFlickablePrivate, public QQuickItemViewTransitionChangeListener, public QAnimationJobChangeListener
{
    Q_DECLARE_PUBLIC(QQuickAbstractItemView)

public:
    QQuickAbstractItemViewPrivate();
    ~QQuickAbstractItemViewPrivate();

    static inline QQuickAbstractItemViewPrivate *get(QQuickAbstractItemView *o) { return o->d_func(); }

    enum BufferMode { NoBuffer = 0x00, BufferBefore = 0x01, BufferAfter = 0x02 };
    enum MovementReason { Other, SetIndex, Mouse };

    bool isValid() const;
    int findLastVisibleIndex(int defaultValue = -1) const;
    FxViewItem *visibleItem(int modelIndex) const;

    virtual void init();
    virtual void clear();
    virtual void updateViewport();
    virtual void updateHeaders();
    virtual void resetPosition();

    virtual void regenerate();
    virtual void layout();
    virtual void refill();
    virtual bool addRemoveVisibleItems();
    virtual void recreateVisibleItems(); // ### TODO: better name

    void animationFinished(QAbstractAnimationJob *) override;
    void mirrorChange() override;

    FxViewItem *createItem(int modelIndex, bool asynchronous = false);
    virtual bool releaseItem(FxViewItem *item);

    QQuickItem *createHighlightItem() const;
    QQuickItem *createComponentItem(QQmlComponent *component, qreal zValue, bool createDefault = false) const;

    static void delegates_append(QQmlListProperty<QQmlDelegate> *prop, QQmlDelegate *delegate);
    static int delegates_count(QQmlListProperty<QQmlDelegate> *prop);
    static QQmlDelegate *delegates_at(QQmlListProperty<QQmlDelegate> *prop, int index);
    static void delegates_clear(QQmlListProperty<QQmlDelegate> *prop);

    void updateCurrent(int modelIndex);
    void updateTrackedItem();
    void updateUnrequestedIndexes();
    void updateUnrequestedPositions();
    void updateVisibleIndex();
    virtual void positionViewAtIndex(int index, int mode) = 0;

    bool createOwnModel();
    void destroyOwnModel();

    void applyPendingChanges();
    virtual bool applyModelChanges() = 0;

    void createTransitioner();
    void prepareVisibleItemTransitions();
    void prepareRemoveTransitions(QHash<QQmlChangeSet::MoveKey, FxViewItem *> *removedItems);
    bool prepareNonVisibleItemTransition(FxViewItem *item, const QRectF &viewBounds);
    void viewItemTransitionFinished(QQuickItemViewTransitionableItem *item) override;

    void markExtentsDirty() {
        if (layoutOrientation() == Qt::Vertical)
            vData.markExtentsDirty();
        else
            hData.markExtentsDirty();
    }

    bool hasPendingChanges() const {
        return currentChanges.hasPendingChanges()
                || bufferedChanges.hasPendingChanges()
                ||runDelayedRemoveTransition;
    }

    void refillOrLayout() {
        if (hasPendingChanges())
            layout();
        else
            refill();
    }

    void forceLayoutPolish() {
        Q_Q(QQuickAbstractItemView);
        forceLayout = true;
        q->polish();
    }

    void releaseVisibleItems() {
        // make a copy and clear the visibleItems first to avoid destroyed
        // items being accessed during the loop (QTBUG-61294)
        const QList<FxViewItem *> oldVisible = visibleItems;
        visibleItems.clear();
        for (FxViewItem *item : oldVisible)
            releaseItem(item);
    }

    QPointer<QQmlInstanceModel> model;
    QVariant modelVariant;
    int itemCount;
    int buffer;
    int bufferMode;
    Qt::LayoutDirection layoutDirection;
    QQuickAbstractItemView::VerticalLayoutDirection verticalLayoutDirection;

    MovementReason moveReason;

    QList<FxViewItem *> visibleItems;
    int visibleIndex;
    int currentIndex;
    FxViewItem *currentItem;
    FxViewItem *trackedItem;
    QHash<QQuickItem*,int> unrequestedItems;
    int requestedIndex;
    QQuickItemViewChangeSet currentChanges;
    QQuickItemViewChangeSet bufferedChanges;
    QPauseAnimationJob bufferPause;

    QQmlComponent *highlightComponent;
    FxViewItem *highlight;
    int highlightRange;     // enum value
    qreal highlightRangeStart;
    qreal highlightRangeEnd;
    int highlightMoveDuration;

    QQuickItemViewTransitioner *transitioner;
    QList<FxViewItem *> releasePendingTransition;

    bool ownModel : 1;
    bool wrap : 1;
    bool keyNavigationEnabled : 1;
    bool explicitKeyNavigationEnabled : 1;
    bool inLayout : 1;
    bool inViewportMoved : 1;
    bool forceLayout : 1;
    bool currentIndexCleared : 1;
    bool haveHighlightRange : 1;
    bool autoHighlight : 1;
    bool highlightRangeStartValid : 1;
    bool highlightRangeEndValid : 1;
    bool fillCacheBuffer : 1;
    bool inRequest : 1;
    bool runDelayedRemoveTransition : 1;
    bool delegateValidated : 1;

protected:
    virtual Qt::Orientation layoutOrientation() const = 0;
    virtual bool isContentFlowReversed() const = 0;

    virtual void createHighlight() = 0;
    virtual void updateHighlight() = 0;
    virtual void resetHighlightPosition() = 0;

    virtual void fixupPosition() = 0;

    virtual void visibleItemsChanged() {}

    virtual FxViewItem *newViewItem(int index, QQuickItem *item) = 0;
    virtual void repositionItemAt(FxViewItem *item, int index, qreal sizeBuffer) = 0;
    virtual void repositionPackageItemAt(QQuickItem *item, int index) = 0;

    virtual void layoutVisibleItems(int fromModelIndex = 0) = 0;
    virtual void changedVisibleIndex(int newIndex) = 0;

    virtual bool needsRefillForAddedOrRemovedIndex(int) const { return false; }
    virtual void translateAndTransitionFilledItems() = 0;

    virtual void initializeViewItem(FxViewItem *) {}
    virtual void initializeCurrentItem() {}
    virtual void updateSectionCriteria() {}
    virtual void updateSections() {}

    void itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &) override;
};

QT_END_NAMESPACE

#endif // QQUICKABSTRACTITEMVIEW_P_P_H
