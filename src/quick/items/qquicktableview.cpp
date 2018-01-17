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

#include "qquicktableview_p.h"
#include "qquickabstractitemview_p_p.h"

#include <QtQml/private/qqmldelegatemodel_p.h>
#include <QtQml/private/qqmlincubator_p.h>

#include <QtCore/qqueue.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcTableViewLayout, "qt.quick.tableview.layout")

#define Q_TABLEVIEW_UNREACHABLE(output) dumpTable(); qDebug() << "output:" << output; Q_UNREACHABLE();
#define Q_TABLEVIEW_ASSERT(cond, output) Q_ASSERT(cond || [&](){ dumpTable(); qDebug() << "output:" << output; return false;}())

class FxTableItemSG : public FxViewItem
{
public:
    FxTableItemSG(QQuickItem *i, QQuickTableView *v, bool own) : FxViewItem(i, v, own, static_cast<QQuickItemViewAttached*>(qmlAttachedPropertiesObject<QQuickTableView>(i)))
    {
    }

    bool contains(qreal x, qreal y) const override
    {
        return x >= itemX() && x < itemX() + itemWidth() &&
               y >= itemY() && y < itemY() + itemHeight();
    }

    void setPosition(const QPointF &pos, bool immediate = false)
    {
        moveTo(pos, immediate); // ###
    }

    QRectF rect() const
    {
        return QRectF(item->position(), item->size());
    }
};

class TableSectionLoadRequest
{
public:
    QPoint startCell;
    QPoint fillDirection;
    QPoint requestedCell = kNullCell;
    QRect loadedTableWhenComplete;
    QQmlIncubator::IncubationMode incubationMode = QQmlIncubator::AsynchronousIfNested;
    bool completed = false;
};

QDebug operator<<(QDebug dbg, const TableSectionLoadRequest request) {
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "TableSectionLoadRequest(";
    dbg << "start:" << request.startCell;
    dbg << " direction:" << request.fillDirection;
    dbg << " incubation:" << request.incubationMode;
    dbg << " targetTableLayout:" << request.loadedTableWhenComplete;
    dbg << " completed:" << request.completed;
    dbg << ')';
    return dbg;
}

class QQuickTableViewPrivate : public QQuickAbstractItemViewPrivate
{
    Q_DECLARE_PUBLIC(QQuickTableView)

public:
    enum LayoutState {
        Idle,
        ProcessingLoadRequest
    };

    QQuickTableViewPrivate();

    bool addRemoveVisibleItems() override;
    void positionViewAtIndex(int index, int mode) override { Q_UNUSED(index); Q_UNUSED(mode); }
    bool applyModelChanges() override { return false; }
    Qt::Orientation layoutOrientation() const override;
    bool isContentFlowReversed() const override;
    void createHighlight() override { }
    void updateHighlight() override { }
    void resetHighlightPosition() override { }
    void fixupPosition() override { }
    void visibleItemsChanged() override;
    FxViewItem *newViewItem(int index, QQuickItem *item) override;
    void repositionItemAt(FxViewItem *item, int index, qreal sizeBuffer) override { Q_UNUSED(item); Q_UNUSED(index); Q_UNUSED(sizeBuffer); }
    void repositionPackageItemAt(QQuickItem *item, int index) override { Q_UNUSED(item); Q_UNUSED(index); }
    void layoutVisibleItems(int fromModelIndex = 0) override { Q_UNUSED(fromModelIndex); }
    void changedVisibleIndex(int newIndex) override { Q_UNUSED(newIndex); }
    void translateAndTransitionFilledItems() override { }

protected:
    QRect loadedTable;

    int rowCount;
    int columnCount;

    QQuickTableView::Orientation orientation;
    qreal rowSpacing;
    qreal columnSpacing;
    TableSectionLoadRequest loadRequest;

    QSizeF accurateContentSize;
    bool contentWidthSetExplicit;
    bool contentHeightSetExplicit;

    bool modified;
    bool blockCreatedItemsSyncCallback;

    QString forcedIncubationMode;

    constexpr static QPoint kLeft = QPoint(-1, 0);
    constexpr static QPoint kRight = QPoint(1, 0);
    constexpr static QPoint kUp = QPoint(0, -1);
    constexpr static QPoint kDown = QPoint(0, 1);

protected:
    QRectF viewportRect() const;
    bool isRightToLeft() const;
    bool isBottomToTop() const;

    int rowAtIndex(int index) const;
    int columnAtIndex(int index) const;
    int indexAt(const QPoint &cellCoord) const;

    inline QPoint coordAt(int index) const;
    inline QPoint coordAt(const FxTableItemSG *tableItem) const;

    FxTableItemSG *loadedTableItem(int modelIndex) const;
    inline FxTableItemSG *loadedTableItem(const QPoint &cellCoord) const;
    inline FxTableItemSG *topRightItem() const;
    inline FxTableItemSG *bottomLeftItem() const;
    inline FxTableItemSG *itemNextTo(const FxTableItemSG *fxViewItem, const QPoint &direction) const;
    inline FxTableItemSG *edgeItem(const FxTableItemSG *fxTableItem, const QRect &loadedTable, Qt::Orientation orientation) const;

    inline bool isEdgeItem(const FxTableItemSG *fxTableItem, const QRect &loadedTable, Qt::Orientation orientation) const;

    FxTableItemSG *loadTableItem(const QPoint &cellCoord, QQmlIncubator::IncubationMode incubationMode);
    inline void showTableItem(FxTableItemSG *fxViewItem);

    bool canFitMoreItemsInDirection(const QPoint &cellCoord, const QPoint &direction, const QRectF fillRect) const;
    bool viewportIsAtTableLayoutEdge();

    qreal calculateItemX(const FxTableItemSG *fxTableItem, const QRect &loadedTable) const;
    qreal calculateItemY(const FxTableItemSG *fxTableItem, const QRect &loadedTable) const;
    qreal calculateItemWidth(const FxTableItemSG *fxTableItem, const QRect &loadedTable) const;
    qreal calculateItemHeight(const FxTableItemSG *fxTableItem, const QRect &loadedTable) const;

    void calculateItemGeometry(FxTableItemSG *fxTableItem, const QRect &loadedTable);
    void calculateContentSize(const FxTableItemSG *fxTableItem);

    void loadInitialTopLeftItem();
    void loadItemsInsideRect(const QRectF &fillRect, QQmlIncubator::IncubationMode incubationMode);
    void unloadItemsOutsideRect(const QRectF &rect);
    void unloadItems(const QPoint &fromCell, const QPoint &toCell);

    void forceCompleteCurrentRequestIfNeeded();
    inline void processCurrentLoadRequest(FxTableItemSG *loadedItem);
    void loadAndUnloadTableItems();
    bool loadTableItemsOneByOne(TableSectionLoadRequest &request, FxTableItemSG *loadedItem);
    void insertItemIntoTable(FxTableItemSG *fxTableItem);

    inline QString tableLayoutToString() const;

    void dumpTable() const;
};

QQuickTableViewPrivate::QQuickTableViewPrivate()
    : loadedTable(QRect())
    , rowCount(-1)
    , columnCount(-1)
    , orientation(QQuickTableView::Vertical)
    , rowSpacing(0)
    , columnSpacing(0)
    , loadRequest(TableSectionLoadRequest())
    , accurateContentSize(QSizeF(-1, -1))
    , contentWidthSetExplicit(false)
    , contentHeightSetExplicit(false)
    , modified(false)
    , blockCreatedItemsSyncCallback(false)
    , forcedIncubationMode(qEnvironmentVariable("QT_TABLEVIEW_INCUBATION_MODE"))
{
}

QRectF QQuickTableViewPrivate::viewportRect() const
{
    Q_Q(const QQuickTableView);
    return QRectF(QPointF(q->contentX(), q->contentY()), q->size());
}

bool QQuickTableViewPrivate::isRightToLeft() const
{
    Q_Q(const QQuickTableView);
    return orientation == QQuickTableView::Horizontal && q->effectiveLayoutDirection() == Qt::RightToLeft;
}

bool QQuickTableViewPrivate::isBottomToTop() const
{
    return orientation == QQuickTableView::Vertical && verticalLayoutDirection == QQuickAbstractItemView::BottomToTop;
}

int QQuickTableViewPrivate::rowAtIndex(int index) const
{
    // Consider factoring this out to a common function, e.g inside QQmlDelegateModel, so that
    // VDMAbstractItemModelDataType can access it as well.

    // Note that we for now rely on the property "rows". But when a proper TableModel is assigned
    // as model, then we should use rowCount from the model if not rows is specified.

    // The trick here is that we convert row and col into an integer/index. This so we can pass
    // that index to the model, regardless of what that model might be (e.g a ListModel). But
    // if the model happens to be a TableModel, or QAbstractItemModel/VDMAbstractItemModelDataType, we
    // then convert the index back to row/col inside there later.

    Q_Q(const QQuickTableView);
    int columns = q->columns();
    if (index < 0 || columns <= 0)
        return -1;
    return index / columns;
}

int QQuickTableViewPrivate::columnAtIndex(int index) const
{
    Q_Q(const QQuickTableView);
    int columns = q->columns();
    if (index < 0 || columns <= 0)
        return -1;
    return index % columns;
}

QPoint QQuickTableViewPrivate::coordAt(int index) const
{
    return QPoint(columnAtIndex(index), rowAtIndex(index));
}

QPoint QQuickTableViewPrivate::coordAt(const FxTableItemSG *tableItem) const
{
    return coordAt(tableItem->index);
}

int QQuickTableViewPrivate::indexAt(const QPoint& cellCoord) const
{
    Q_Q(const QQuickTableView);
    int rows = q->rows();
    int columns = q->columns();
    if (cellCoord.y() < 0 || cellCoord.y() >= rows || cellCoord.x() < 0 || cellCoord.x() >= columns)
        return -1;
    return cellCoord.x() + cellCoord.y() * columns;
}

FxTableItemSG *QQuickTableViewPrivate::loadedTableItem(int modelIndex) const
{
    // TODO: this is an overload of visibleItems, since the other version
    // does some wierd things with the index. Fix that up later. And change
    // visibleItems to use hash or map instead of list.

    if (modelIndex == -1)
        return nullptr;

    for (int i = 0; i < visibleItems.count(); ++i) {
        FxViewItem *item = visibleItems.at(i);
        if (item->index == modelIndex)
            return static_cast<FxTableItemSG *>(item);
    }

    return nullptr;
}

FxTableItemSG *QQuickTableViewPrivate::loadedTableItem(const QPoint &cellCoord) const
{
    return loadedTableItem(indexAt(cellCoord));
}

FxTableItemSG *QQuickTableViewPrivate::topRightItem() const
{
    return loadedTableItem(loadedTable.topRight());
}

FxTableItemSG *QQuickTableViewPrivate::bottomLeftItem() const
{
    return loadedTableItem(loadedTable.bottomLeft());
}

FxTableItemSG *QQuickTableViewPrivate::edgeItem(const FxTableItemSG *fxTableItem, const QRect &tableRect, Qt::Orientation orientation) const
{
    return orientation == Qt::Horizontal
        ? loadedTableItem(QPoint(coordAt(fxTableItem).x(), tableRect.topLeft().y()))
        : loadedTableItem(QPoint(tableRect.topLeft().x(), coordAt(fxTableItem).y()));
}

bool QQuickTableViewPrivate::isEdgeItem(const FxTableItemSG *fxTableItem, const QRect &tableRect, Qt::Orientation orientation) const
{
    // An edge item is an item that is on the same row or column as the top left item. But
    // depending on the information needed, we sometimes don't consider an item as en edge
    // item horizontally when laying out vertically and vica versa.
    return orientation == Qt::Vertical
            ? coordAt(fxTableItem).x() == tableRect.topLeft().x()
            : coordAt(fxTableItem).y() == tableRect.topLeft().y();
}

void QQuickTableViewPrivate::calculateContentSize(const FxTableItemSG *fxTableItem)
{
    Q_Q(QQuickTableView);

    if (!contentWidthSetExplicit && accurateContentSize.width() == -1) {
        if (loadedTable.topRight().x() == columnCount - 1) {
            // We are at the end, and can determine accurate content width
            if (const FxTableItemSG *topRightItem = loadedTableItem(loadedTable.topRight())) {
                accurateContentSize.setWidth(topRightItem->rect().right());
                q->setContentWidth(accurateContentSize.width());
            }
        } else {
            // We don't know what the width of the content view will be
            // at this time, so we just set it to be a little bigger than
            // the edge of the right-most item.
            const QRectF &itemRect = fxTableItem->rect();
            qreal suggestedContentWidth = itemRect.right() + 500;
            if (suggestedContentWidth > q->contentWidth())
                q->setContentWidth(suggestedContentWidth);
        }
    }

    if (!contentHeightSetExplicit && accurateContentSize.height() == -1) {
        if (loadedTable.bottomRight().y() == rowCount - 1) {
            if (const FxTableItemSG *bottomLeftItem = loadedTableItem(loadedTable.bottomLeft())) {
                accurateContentSize.setHeight(bottomLeftItem->rect().bottom());
                q->setContentHeight(accurateContentSize.height());
            }
        } else {
            const QRectF &itemRect = fxTableItem->rect();
            qreal suggestedContentHeight = itemRect.bottom() + 500;
            if (suggestedContentHeight > q->contentHeight())
                q->setContentHeight(suggestedContentHeight);
        }
    }
}

Qt::Orientation QQuickTableViewPrivate::layoutOrientation() const
{
    return static_cast<Qt::Orientation>(orientation);
}

bool QQuickTableViewPrivate::isContentFlowReversed() const
{
    return isRightToLeft() || isBottomToTop();
}

void QQuickTableViewPrivate::visibleItemsChanged()
{
}

FxViewItem *QQuickTableViewPrivate::newViewItem(int index, QQuickItem *item)
{
    Q_Q(QQuickTableView);
    Q_UNUSED(index);
    return new FxTableItemSG(item, q, false);
}

QString QQuickTableViewPrivate::tableLayoutToString() const
{
    return QString(QLatin1String("current table: (%1,%2) -> (%3,%4), item count: %5"))
            .arg(loadedTable.topLeft().x()).arg(loadedTable.topLeft().y())
            .arg(loadedTable.bottomRight().x()).arg(loadedTable.bottomRight().y())
            .arg(visibleItems.count());
}

void QQuickTableViewPrivate::dumpTable() const
{
    qDebug() << "******* TABLE DUMP *******";
    auto listCopy = visibleItems;
    std::stable_sort(listCopy.begin(), listCopy.end(), [](const FxViewItem *lhs, const FxViewItem *rhs) { return lhs->index < rhs->index; });

    for (int i = 0; i < listCopy.count(); ++i) {
        FxTableItemSG *item = static_cast<FxTableItemSG *>(listCopy.at(i));
        qDebug() << coordAt(item);
    }

    qDebug() << tableLayoutToString();

    QString filename = QStringLiteral("qquicktableview_dumptable_capture.png");
    if (q_func()->window()->grabWindow().save(filename))
        qDebug() << "Window capture saved to:" << filename;
}

FxTableItemSG *QQuickTableViewPrivate::loadTableItem(const QPoint &cellCoord, QQmlIncubator::IncubationMode incubationMode)
{
    qCDebug(lcItemViewDelegateLifecycle) << cellCoord;

    // Note: at this point before rebase, sync really means AsyncIfNested
    static const bool forcedAsync = forcedIncubationMode == QLatin1String("async");
    bool async = forcedAsync || incubationMode == QQmlIncubator::Asynchronous;

    QBoolBlocker guard(blockCreatedItemsSyncCallback);
    auto loadedItem = static_cast<FxTableItemSG *>(createItem(indexAt(cellCoord), async));

    if (loadedItem) {
        // This path can happen even if incubationMode == Asynchronous, since
        // the item can be cached and ready in the model from before.
        qCDebug(lcItemViewDelegateLifecycle) << "item received synchronously:" << cellCoord;
    }

    return loadedItem;
}

void QQuickTableViewPrivate::unloadItems(const QPoint &fromCell, const QPoint &toCell)
{
    qCDebug(lcItemViewDelegateLifecycle) << fromCell << "->" << toCell;
    modified = true;

    for (int y = fromCell.y(); y <= toCell.y(); ++y) {
        for (int x = fromCell.x(); x <= toCell.x(); ++x) {
            FxTableItemSG *item = loadedTableItem(QPoint(x ,y));
            visibleItems.removeOne(item);
            releaseItem(item);
        }
    }
}

void QQuickTableViewPrivate::showTableItem(FxTableItemSG *fxViewItem)
{
    QQuickItemPrivate::get(fxViewItem->item)->setCulled(false);
}

FxTableItemSG *QQuickTableViewPrivate::itemNextTo(const FxTableItemSG *fxViewItem, const QPoint &direction) const
{
    return loadedTableItem(coordAt(fxViewItem) + direction);
}

bool QQuickTableViewPrivate::canFitMoreItemsInDirection(const QPoint &cellCoord, const QPoint &direction, const QRectF fillRect) const
{
    if (direction.isNull())
        return false;

    Q_TABLEVIEW_ASSERT(loadedTableItem(cellCoord), cellCoord);
    const QRectF itemRect = loadedTableItem(cellCoord)->rect();

    if (direction == kRight) {
        if (cellCoord.x() == columnCount - 1)
            return false;
        return itemRect.right() < fillRect.right();
    } else if (direction == kLeft) {
        if (cellCoord.x() == 0)
            return false;
        return itemRect.left() > fillRect.left();
    } else if (direction == kDown) {
        if (cellCoord.y() == rowCount - 1)
            return false;
        return itemRect.bottom() < fillRect.bottom();
    } else if (direction == kUp) {
        if (cellCoord.y() == 0)
            return false;
        return itemRect.top() > fillRect.top();
    } else {
        Q_TABLEVIEW_UNREACHABLE(cellCoord << direction);
    }

    return false;
}

bool QQuickTableViewPrivate::viewportIsAtTableLayoutEdge()
{
    const QRectF &visibleRect = viewportRect();

    auto topLeftItem = loadedTableItem(loadedTable.topLeft());
    Q_TABLEVIEW_ASSERT(topLeftItem, loadedTable.topLeft());
    const QRectF topLeftRect = topLeftItem->rect();

    if (visibleRect.left() < topLeftRect.left())
        return true;
    if (visibleRect.top() < topLeftRect.top())
        return true;

    auto bottomRightItem = loadedTableItem(loadedTable.bottomRight());
    Q_TABLEVIEW_ASSERT(bottomRightItem, loadedTable.bottomRight());
    const QRectF bottomRightRect = bottomRightItem->rect();

    if (visibleRect.right() > bottomRightRect.right())
        return true;
    if (visibleRect.bottom() > bottomRightRect.bottom())
        return true;

    return false;
}

qreal QQuickTableViewPrivate::calculateItemX(const FxTableItemSG *fxTableItem, const QRect &tableRect) const
{
    // Calculate the table position x of fxViewItem based on the position of the item on
    // the horizontal edge, or otherwise the item directly next to it. This assumes that at
    // least the item next to it has already been loaded. If the edge item has been
    // loaded, we use that as reference instead, as it allows us to load several items in parallel.
    if (visibleItems.size() == 1) {
        // Special case: the first item will be at 0, 0 for now
        return 0;
    }

    if (isEdgeItem(fxTableItem, tableRect, Qt::Horizontal)) {
        // For edge items we can find the X position by looking at any adjacent item
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kUp))
            return neighborItem->rect().x();
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kDown))
            return neighborItem->rect().x();
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kLeft))
            return neighborItem->rect().right() + columnSpacing;
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kRight))
            return neighborItem->rect().left() - columnSpacing - fxTableItem->rect().width();
        Q_TABLEVIEW_UNREACHABLE(coordAt(fxTableItem));
    } else {
        auto edge = edgeItem(fxTableItem, tableRect, Qt::Horizontal);
        Q_TABLEVIEW_ASSERT(edge, coordAt(fxTableItem));
        return edge->rect().x();
    }
}

qreal QQuickTableViewPrivate::calculateItemY(const FxTableItemSG *fxTableItem, const QRect &tableRect) const
{
    // Calculate the table position y of fxViewItem based on the position of the item on
    // the vertical edge, or otherwise the item directly next to it. This assumes that at
    // least the item next to it has already been loaded. If the edge item has been
    // loaded, we use that as reference instead, as it allows us to load several items in parallel.
    if (visibleItems.size() == 1) {
        // Special case: the first item will be at 0, 0 for now
        return 0;
    }

    if (isEdgeItem(fxTableItem, tableRect, Qt::Vertical)) {
        // For edge items we can find the Y position by looking at any adjacent item
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kLeft))
            return neighborItem->rect().y();
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kRight))
            return neighborItem->rect().y();
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kUp))
            return neighborItem->rect().bottom() + rowSpacing;
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kDown))
            return neighborItem->rect().top() - rowSpacing - fxTableItem->rect().height();
        Q_TABLEVIEW_UNREACHABLE(coordAt(fxTableItem));
    } else {
        auto edge = edgeItem(fxTableItem, tableRect, Qt::Vertical);
        Q_TABLEVIEW_ASSERT(edge, coordAt(fxTableItem));
        return edge->rect().y();
    }
}

qreal QQuickTableViewPrivate::calculateItemWidth(const FxTableItemSG *fxTableItem, const QRect &tableRect) const
{
    if (visibleItems.size() == 1) {
        return fxTableItem->rect().width();
    }

    if (isEdgeItem(fxTableItem, tableRect, Qt::Horizontal)) {
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kUp))
            return neighborItem->rect().width();
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kDown))
            return neighborItem->rect().width();
        return fxTableItem->rect().width();
    } else {
        auto edge = edgeItem(fxTableItem, tableRect, Qt::Horizontal);
        Q_TABLEVIEW_ASSERT(edge, coordAt(fxTableItem));
        return edge->rect().width();
    }
}

qreal QQuickTableViewPrivate::calculateItemHeight(const FxTableItemSG *fxTableItem, const QRect &tableRect) const
{
    if (visibleItems.size() == 1) {
        return fxTableItem->rect().height();
    }

    if (isEdgeItem(fxTableItem, tableRect, Qt::Vertical)) {
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kLeft))
            return neighborItem->rect().height();
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kRight))
            return neighborItem->rect().height();
        return fxTableItem->rect().height();
    } else {
        auto edge = edgeItem(fxTableItem, tableRect, Qt::Vertical);
        Q_TABLEVIEW_ASSERT(edge, coordAt(fxTableItem));
        return edge->rect().height();
    }
}

void QQuickTableViewPrivate::calculateItemGeometry(FxTableItemSG *fxTableItem, const QRect &tableRect)
{
    qreal w = calculateItemWidth(fxTableItem, tableRect);
    qreal h = calculateItemHeight(fxTableItem, tableRect);
    fxTableItem->setSize(QSizeF(w, h), true);

    qreal x = calculateItemX(fxTableItem, tableRect);
    qreal y = calculateItemY(fxTableItem, tableRect);
    fxTableItem->setPosition(QPointF(x, y));

    qCDebug(lcItemViewDelegateLifecycle()) << coordAt(fxTableItem) << fxTableItem->rect();
}

void QQuickTableView::createdItem(int index, QObject*)
{
    Q_D(QQuickTableView);

    if (index == d->requestedIndex) {
        // This is needed by the model.
        // TODO: let model have it's own handler, and refactor requestedIndex out the view?
        d->requestedIndex = -1;
    }

    if (d->blockCreatedItemsSyncCallback) {
        // Items that are created synchronously will still need to
        // be delivered async later, to funnel all item creation through
        // the same async layout logic.
        return;
    }

    qCDebug(lcItemViewDelegateLifecycle) << "loaded asynchronously:" << d->coordAt(index);

    // It's important to use createItem to get the item, and
    // not the object argument, since the former will ref-count it.
    FxTableItemSG *item = static_cast<FxTableItemSG *>(d->createItem(index, false));

    Q_ASSERT(item);
    Q_ASSERT(item->item);

    d->processCurrentLoadRequest(item);
    d->loadAndUnloadTableItems();
}

void QQuickTableViewPrivate::insertItemIntoTable(FxTableItemSG *fxTableItem)
{
    qCDebug(lcItemViewDelegateLifecycle) << coordAt(fxTableItem);
    modified = true;

    visibleItems.append(fxTableItem);
    calculateItemGeometry(fxTableItem, loadRequest.loadedTableWhenComplete);
    calculateContentSize(fxTableItem);
    showTableItem(fxTableItem);
}

void QQuickTableViewPrivate::forceCompleteCurrentRequestIfNeeded()
{
    if (loadRequest.completed)
        return;

    if (loadRequest.incubationMode == QQmlIncubator::AsynchronousIfNested)
        return;

    if (!viewportIsAtTableLayoutEdge())
        return;

    loadRequest.incubationMode = QQmlIncubator::AsynchronousIfNested;
    processCurrentLoadRequest(nullptr);
}

void QQuickTableViewPrivate::processCurrentLoadRequest(FxTableItemSG *loadedItem)
{
    QPoint horizontalFillDirection = QPoint(loadRequest.fillDirection.x(), 0);
    QPoint verticalFillDirection = QPoint(0, loadRequest.fillDirection.y());
    bool loadHorizontal = !horizontalFillDirection.isNull();
    bool loadVertical = !verticalFillDirection.isNull();

    if (!loadedItem) {
        if (loadRequest.requestedCell == kNullCell) {
            qCDebug(lcItemViewDelegateLifecycle()) << "begin:" << loadRequest;
            loadRequest.requestedCell = loadRequest.startCell;
        } else {
            // This path should only be taken when we need to force complete
            Q_TABLEVIEW_ASSERT(loadRequest.incubationMode == QQmlIncubator::AsynchronousIfNested, loadRequest);
            qCDebug(lcItemViewDelegateLifecycle()) << "force complete:" << loadRequest;
        }

        loadedItem = loadTableItem(loadRequest.requestedCell, loadRequest.incubationMode);
    }

    while (loadedItem) {
        insertItemIntoTable(loadedItem);

        // Continue loading items in the request, until one of them ends up loading async.
        // If so, we just return, and wait for this function to be called again once the
        // requested item is ready.
        const QPoint loadedCoord = coordAt(loadedItem);

        if (loadHorizontal && loadRequest.loadedTableWhenComplete.contains(loadedCoord + horizontalFillDirection)) {
            loadRequest.requestedCell = loadedCoord + horizontalFillDirection;
        } else if (loadVertical && loadRequest.loadedTableWhenComplete.contains(loadedCoord + verticalFillDirection)) {
            loadRequest.requestedCell = QPoint(loadRequest.startCell.x(), loadedCoord.y() + 1);
        } else {
            loadRequest.completed = true;
            loadedTable = loadRequest.loadedTableWhenComplete;
            qCDebug(lcItemViewDelegateLifecycle()) << "completed:" << loadRequest;
            qCDebug(lcTableViewLayout()) << tableLayoutToString();
            return;
        }

        loadedItem = loadTableItem(loadRequest.requestedCell, loadRequest.incubationMode);
    }
}

void QQuickTableViewPrivate::loadInitialTopLeftItem()
{
    // Load top-left item. After loaded, processLoadRequest() will take
    // care of filling out the rest of the table.
    Q_TABLEVIEW_ASSERT(visibleItems.isEmpty(), visibleItems.count());
    Q_TABLEVIEW_ASSERT(loadedTable.isEmpty(), loadedTable);

    QPoint newTopLeft(0, 0);

    loadRequest = TableSectionLoadRequest();
    loadRequest.loadedTableWhenComplete = QRect(newTopLeft, newTopLeft);
    loadRequest.startCell = newTopLeft;
    loadRequest.incubationMode = QQmlIncubator::AsynchronousIfNested;
    qCDebug(lcTableViewLayout()) << "load top-left:" << newTopLeft;
    processCurrentLoadRequest(nullptr);
}

void QQuickTableViewPrivate::unloadItemsOutsideRect(const QRectF &rect)
{
    // Unload as many rows and columns as possible outside rect. But we
    // never unload the row or column that contains top-left (e.g if the
    // user flicks at the end of the table), since then we would lose our
    // anchor point for layouting, which we need once rows and columns are
    // flicked back into view again.
    bool itemsUnloaded;

    do {
        itemsUnloaded = false;

        Q_TABLEVIEW_ASSERT(loadedTableItem(loadedTable.topLeft()), rect);
        Q_TABLEVIEW_ASSERT(loadedTableItem(loadedTable.bottomRight()), rect);

        const QRectF &topLeftRect = loadedTableItem(loadedTable.topLeft())->rect();
        const QRectF &bottomRightRect = loadedTableItem(loadedTable.bottomRight())->rect();
        const qreal floatingPointMargin = 1;

        if (loadedTable.width() > 1) {
            if (rect.left() > topLeftRect.right() + floatingPointMargin) {
                qCDebug(lcTableViewLayout()) << "unload left column" << loadedTable.topLeft().x();
                unloadItems(loadedTable.topLeft(), loadedTable.bottomLeft());
                loadedTable.adjust(1, 0, 0, 0);
                itemsUnloaded = true;
            } else if (bottomRightRect.left() > rect.right() + floatingPointMargin) {
                qCDebug(lcTableViewLayout()) << "unload right column" << loadedTable.topRight().x();
                unloadItems(loadedTable.topRight(), loadedTable.bottomRight());
                loadedTable.adjust(0, 0, -1, 0);
                itemsUnloaded = true;
            }
        }

        if (loadedTable.height() > 1) {
            if (rect.top() > topLeftRect.bottom() + floatingPointMargin) {
                qCDebug(lcTableViewLayout()) << "unload top row" << loadedTable.topLeft().y();
                unloadItems(loadedTable.topLeft(), loadedTable.topRight());
                loadedTable.adjust(0, 1, 0, 0);
                itemsUnloaded = true;
            } else if (bottomRightRect.top() > rect.bottom() + floatingPointMargin) {
                qCDebug(lcTableViewLayout()) << "unload bottom row" << loadedTable.bottomLeft().y();
                unloadItems(loadedTable.bottomLeft(), loadedTable.bottomRight());
                loadedTable.adjust(0, 0, 0, -1);
                itemsUnloaded = true;
            }
        }

        if (itemsUnloaded)
            qCDebug(lcTableViewLayout()) << tableLayoutToString();

    } while (itemsUnloaded);
}

void QQuickTableViewPrivate::loadItemsInsideRect(const QRectF &fillRect, QQmlIncubator::IncubationMode incubationMode)
{    
    Q_TABLEVIEW_ASSERT(loadRequest.completed, loadRequest);

    do {
        if (canFitMoreItemsInDirection(loadedTable.topLeft(), kLeft, fillRect)) {
            loadRequest = TableSectionLoadRequest();
            loadRequest.loadedTableWhenComplete = loadedTable.adjusted(-1, 0, 0, 0);
            loadRequest.startCell = loadRequest.loadedTableWhenComplete.topLeft();
            loadRequest.fillDirection = kDown;
            loadRequest.incubationMode = incubationMode;
            qCDebug(lcTableViewLayout()) << "load left column" << loadRequest.startCell.x();
            processCurrentLoadRequest(nullptr);
        } else if (canFitMoreItemsInDirection(loadedTable.topRight(), kRight, fillRect)) {
            loadRequest = TableSectionLoadRequest();
            loadRequest.loadedTableWhenComplete = loadedTable.adjusted(0, 0, 1, 0);
            loadRequest.startCell = loadRequest.loadedTableWhenComplete.topRight();
            loadRequest.fillDirection = kDown;
            loadRequest.incubationMode = incubationMode;
            qCDebug(lcTableViewLayout()) << "load right column" << loadRequest.startCell.x();
            processCurrentLoadRequest(nullptr);
        } else if (canFitMoreItemsInDirection(loadedTable.topLeft(), kUp, fillRect)) {
            loadRequest = TableSectionLoadRequest();
            loadRequest.loadedTableWhenComplete = loadedTable.adjusted(0, -1, 0, 0);
            loadRequest.startCell = loadRequest.loadedTableWhenComplete.topLeft();
            loadRequest.fillDirection = kRight;
            loadRequest.incubationMode = incubationMode;
            qCDebug(lcTableViewLayout()) << "load top row" << loadRequest.startCell.y();
            processCurrentLoadRequest(nullptr);
        } else if (canFitMoreItemsInDirection(loadedTable.bottomLeft(), kDown, fillRect)) {
            loadRequest = TableSectionLoadRequest();
            loadRequest.loadedTableWhenComplete = loadedTable.adjusted(0, 0, 0, 1);
            loadRequest.startCell = loadRequest.loadedTableWhenComplete.bottomLeft();
            loadRequest.fillDirection = kRight;
            loadRequest.incubationMode = incubationMode;
            qCDebug(lcTableViewLayout()) << "load bottom row" << loadRequest.startCell.y();
            processCurrentLoadRequest(nullptr);
        } else {
            // Nothing more to load
            return;
        }

    } while (loadRequest.completed);
}

void QQuickTableViewPrivate::loadAndUnloadTableItems()
{
    if (!loadRequest.completed)
        return;

    QRectF visibleRect = viewportRect();
    QRectF bufferRect = visibleRect.adjusted(-buffer, -buffer, buffer, buffer);

    if (!visibleRect.isValid())
        return;

    unloadItemsOutsideRect(bufferRect);
    loadItemsInsideRect(visibleRect, QQmlIncubator::AsynchronousIfNested);

    if (loadRequest.completed && !q_func()->isMoving())
        loadItemsInsideRect(bufferRect, QQmlIncubator::Asynchronous);
}

bool QQuickTableViewPrivate::addRemoveVisibleItems()
{
    modified = false;

    loadAndUnloadTableItems();

    return modified;
}

QQuickTableView::QQuickTableView(QQuickItem *parent)
    : QQuickAbstractItemView(*(new QQuickTableViewPrivate), parent)
{
    connect(this, &QQuickTableView::movingChanged, [=] {
        // When moving stops, we call loadAndUnloadTableItems
        // to start loading items into buffer.
        d_func()->loadAndUnloadTableItems();
    });
}

void QQuickTableView::viewportMoved(Qt::Orientations orient)
{
    Q_D(QQuickTableView);
    QQuickAbstractItemView::viewportMoved(orient);

    // If the viewport moved all the way to the edge of the loaded table
    // items, we need to complete any ongoing async buffer loading now, and
    // focus on loading items that are entering the view instead.
    d->forceCompleteCurrentRequestIfNeeded();

    d->addRemoveVisibleItems();
}

int QQuickTableView::rows() const
{
    Q_D(const QQuickTableView);
    if (QQmlDelegateModel *delegateModel = qobject_cast<QQmlDelegateModel *>(d->model.data()))
        return delegateModel->rows();
    return qMax(0, d->rowCount);
}

void QQuickTableView::setRows(int rows)
{
    Q_D(QQuickTableView);
    if (d->rowCount == rows)
        return;

    d->rowCount = rows;
    if (d->componentComplete && d->model && d->ownModel)
        static_cast<QQmlDelegateModel *>(d->model.data())->setRows(rows);

    emit rowsChanged();
}

void QQuickTableView::resetRows()
{
    Q_D(QQuickTableView);
    if (d->rowCount == -1)
        return;

    d->rowCount = -1;
    if (d->componentComplete && d->model && d->ownModel)
        static_cast<QQmlDelegateModel *>(d->model.data())->resetRows();
    emit rowsChanged();
}

int QQuickTableView::columns() const
{
    return d_func()->columnCount;
}

void QQuickTableView::setColumns(int columns)
{
    Q_D(QQuickTableView);
    if (d->columnCount == columns)
        return;

    d->columnCount = columns;
    if (d->componentComplete && d->model && d->ownModel)
        static_cast<QQmlDelegateModel *>(d->model.data())->setColumns(columns);

    emit columnsChanged();
}

void QQuickTableView::resetColumns()
{
    Q_D(QQuickTableView);
    if (d->columnCount == -1)
        return;

    d->columnCount = -1;
    if (d->componentComplete && d->model && d->ownModel)
        static_cast<QQmlDelegateModel *>(d->model.data())->resetColumns();
    emit columnsChanged();
}

qreal QQuickTableView::rowSpacing() const
{
    Q_D(const QQuickTableView);
    return d->rowSpacing;
}

void QQuickTableView::setRowSpacing(qreal spacing)
{
    Q_D(QQuickTableView);
    if (d->rowSpacing == spacing)
        return;

    d->rowSpacing = spacing;
    d->forceLayoutPolish();
    emit rowSpacingChanged();
}

qreal QQuickTableView::columnSpacing() const
{
    Q_D(const QQuickTableView);
    return d->columnSpacing;
}

void QQuickTableView::setColumnSpacing(qreal spacing)
{
    Q_D(QQuickTableView);
    if (d->columnSpacing == spacing)
        return;

    d->columnSpacing = spacing;
    d->forceLayoutPolish();
    emit columnSpacingChanged();
}

QQuickTableView::Orientation QQuickTableView::orientation() const
{
    Q_D(const QQuickTableView);
    return d->orientation;
}

void QQuickTableView::setOrientation(Orientation orientation)
{
    Q_D(QQuickTableView);
    if (d->orientation == orientation)
        return;

    d->orientation = orientation;
    if (isComponentComplete())
        d->regenerate();
    emit orientationChanged();
}

QQuickTableViewAttached *QQuickTableView::qmlAttachedProperties(QObject *obj)
{
    return new QQuickTableViewAttached(obj);
}

void QQuickTableView::initItem(int index, QObject *object)
{
    QQuickAbstractItemView::initItem(index, object);

    // setting the view from the FxViewItem wrapper is too late if the delegate
    // needs access to the view in Component.onCompleted
    QQuickItem *item = qmlobject_cast<QQuickItem *>(object);
    if (item) {
        QQuickTableViewAttached *attached = static_cast<QQuickTableViewAttached *>(
                qmlAttachedPropertiesObject<QQuickTableView>(item));
        if (attached)
            attached->setView(this);
    }
}

void QQuickTableView::componentComplete()
{
    Q_D(QQuickTableView);
    if (d->model && d->ownModel) {
        QQmlDelegateModel *delegateModel = static_cast<QQmlDelegateModel *>(d->model.data());
        if (d->rowCount > 0)
            delegateModel->setRows(d->rowCount);
        if (d->columnCount > 0)
            delegateModel->setColumns(d->columnCount);
        static_cast<QQmlDelegateModel *>(d->model.data())->componentComplete();
    }

    // Allow app to set content size explicitly, instead of us trying to guess as we go
    d->contentWidthSetExplicit = (contentWidth() != -1);
    d->contentHeightSetExplicit = (contentHeight() != -1);

    d->loadInitialTopLeftItem();

    // NB: deliberatly skipping QQuickAbstractItemView, since it does so
    // many wierd things that I don't need or understand. That code is messy...
    QQuickFlickable::componentComplete();
}

QT_END_NAMESPACE
