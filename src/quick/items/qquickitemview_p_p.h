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

#ifndef QQUICKITEMVIEW_P_P_H
#define QQUICKITEMVIEW_P_P_H

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

#include "qquickitemview_p.h"
#include "qquickabstractitemview_p_p.h"
#include <QtQml/private/qqmldelegatemodel_p.h>


QT_BEGIN_NAMESPACE


class Q_AUTOTEST_EXPORT QQuickItemViewPrivate : public QQuickAbstractItemViewPrivate
{
    Q_DECLARE_PUBLIC(QQuickItemView)
public:
    QQuickItemViewPrivate();

    static inline QQuickItemViewPrivate *get(QQuickItemView *o) { return o->d_func(); }

    struct ChangeResult {
        QQmlNullableValue<qreal> visiblePos;
        bool changedFirstItem;
        qreal sizeChangesBeforeVisiblePos;
        qreal sizeChangesAfterVisiblePos;
        int countChangeBeforeVisible;
        int countChangeAfterVisibleItems;

        ChangeResult()
            : visiblePos(0), changedFirstItem(false),
              sizeChangesBeforeVisiblePos(0), sizeChangesAfterVisiblePos(0),
              countChangeBeforeVisible(0), countChangeAfterVisibleItems(0) {}

        ChangeResult(const QQmlNullableValue<qreal> &p)
            : visiblePos(p), changedFirstItem(false),
              sizeChangesBeforeVisiblePos(0), sizeChangesAfterVisiblePos(0),
              countChangeBeforeVisible(0), countChangeAfterVisibleItems(0) {}

        ChangeResult &operator+=(const ChangeResult &other) {
            if (&other == this)
                return *this;
            changedFirstItem &= other.changedFirstItem;
            sizeChangesBeforeVisiblePos += other.sizeChangesBeforeVisiblePos;
            sizeChangesAfterVisiblePos += other.sizeChangesAfterVisiblePos;
            countChangeBeforeVisible += other.countChangeBeforeVisible;
            countChangeAfterVisibleItems += other.countChangeAfterVisibleItems;
            return *this;
        }

        void reset() {
            changedFirstItem = false;
            sizeChangesBeforeVisiblePos = 0.0;
            sizeChangesAfterVisiblePos = 0.0;
            countChangeBeforeVisible = 0;
            countChangeAfterVisibleItems = 0;
        }
    };

    qreal position() const;
    qreal size() const;
    qreal startPosition() const;
    qreal endPosition() const;

    qreal contentStartOffset() const;
    FxViewItem *firstVisibleItem() const;
    int mapFromModel(int modelIndex) const;

    void init() override;
    void updateViewport() override;
    void updateHeaders() override;
    void resetPosition() override;

    void orientationChange();
    bool addRemoveVisibleItems() override;

    void updateLastIndexInView();
    void positionViewAtIndex(int index, int mode) override;

    qreal minExtentForAxis(const AxisData &axisData, bool forXAxis) const;
    qreal maxExtentForAxis(const AxisData &axisData, bool forXAxis) const;
    qreal calculatedMinExtent() const;
    qreal calculatedMaxExtent() const;

    bool applyModelChanges() override;
    bool applyRemovalChange(const QQmlChangeSet::Change &removal, ChangeResult *changeResult, int *removedCount);
    void removeItem(FxViewItem *item, const QQmlChangeSet::Change &removal, ChangeResult *removeResult);
    virtual void updateSizeChangesBeforeVisiblePos(FxViewItem *item, ChangeResult *removeResult);
    void repositionFirstItem(FxViewItem *prevVisibleItemsFirst, qreal prevVisibleItemsFirstPos,
            FxViewItem *prevFirstVisible, ChangeResult *insertionResult, ChangeResult *removalResult);

    void checkVisible() const;
    void showVisibleItems() const;

    int displayMarginBeginning;
    int displayMarginEnd;

    QQmlComponent *headerComponent;
    FxViewItem *header;
    QQmlComponent *footerComponent;
    FxViewItem *footer;

    struct MovedItem {
        FxViewItem *item;
        QQmlChangeSet::MoveKey moveKey;
        MovedItem(FxViewItem *i, QQmlChangeSet::MoveKey k)
            : item(i), moveKey(k) {}
    };

    ChangeResult insertionPosChanges;
    ChangeResult removalPosChanges;

    mutable qreal minExtent;
    mutable qreal maxExtent;

    int lastIndexInView;

protected:
    virtual qreal positionAt(int index) const = 0;
    virtual qreal endPositionAt(int index) const = 0;
    virtual qreal originPosition() const = 0;
    virtual qreal lastPosition() const = 0;

    // these are positions and sizes along the current direction of scrolling/flicking
    virtual qreal itemPosition(FxViewItem *item) const = 0;
    virtual qreal itemEndPosition(FxViewItem *item) const = 0;
    virtual qreal itemSize(FxViewItem *item) const = 0;
    virtual qreal itemSectionSize(FxViewItem *item) const = 0;

    virtual qreal headerSize() const = 0;
    virtual qreal footerSize() const = 0;
    virtual bool showHeaderForIndex(int index) const = 0;
    virtual bool showFooterForIndex(int index) const = 0;
    virtual void updateHeader() = 0;
    virtual void updateFooter() = 0;
    virtual bool hasStickyHeader() const { return false; }
    virtual bool hasStickyFooter() const { return false; }

    virtual void setPosition(qreal pos) = 0;

    virtual bool addVisibleItems(qreal fillFrom, qreal fillTo, qreal bufferFrom, qreal bufferTo, bool doBuffer) = 0;
    virtual bool removeNonVisibleItems(qreal bufferFrom, qreal bufferTo) = 0;

    virtual void resetFirstItemPosition(qreal pos = 0.0) = 0;
    virtual void adjustFirstItem(qreal forwards, qreal backwards, int changeBeforeVisible) = 0;

    virtual bool applyInsertionChange(const QQmlChangeSet::Change &insert, ChangeResult *changeResult,
                QList<FxViewItem *> *newItems, QList<MovedItem> *movingIntoView) = 0;

    void itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &) override;
};


QT_END_NAMESPACE

#endif // QQUICKITEMVIEW_P_P_H
