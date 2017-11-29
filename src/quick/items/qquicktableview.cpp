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
};

class TableSectionLoadRequest
{
public:
    enum LoadMode {
        LoadOneByOne,
        LoadInParallel
    };

    QPoint startCell = QPoint(0, 0);
    QPoint fillDirection = QPoint(0, 0);
    LoadMode loadMode = LoadOneByOne;
    int requestedItemCount = 0;
    bool done = false;

    bool onlyOneItemRequested() const { return fillDirection.isNull(); }
};

QDebug operator<<(QDebug dbg, const TableSectionLoadRequest request) {
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "TableSectionLoadRequest(";
    dbg << "start:" << request.startCell;
    dbg << " direction:" << request.fillDirection;
    dbg << " mode:" << request.loadMode;
    dbg << " done:" << request.done;
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

    bool isRightToLeft() const;
    bool isBottomToTop() const;

    int rowAtIndex(int index) const;
    int rowAtPos(qreal y) const;
    int columnAtIndex(int index) const;
    int columnAtPos(qreal x) const;
    int indexAt(int row, int column) const;
    int indexAt(const QPoint &cellCoord) const;
    QPoint cellCoordAt(int index) const;
    FxViewItem *visibleItemAt(int row, int column) const;

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
    LayoutState layoutState;
    QRectF currentLayoutRect;
    int currentTopLeftIndex;
    int currentTopRightColumn;
    int currentBottomLeftRow;

    int rowCount;
    int columnCount;

    QQuickTableView::Orientation orientation;
    qreal rowSpacing;
    qreal columnSpacing;
    QQueue<TableSectionLoadRequest> loadRequests;
    QEvent::Type eventTypeDeliverPostedTableItems;
    QVector<int> postedTableItems;
    bool blockCreatedItemsSyncCallback;
    bool forceSynchronousMode;
    bool inViewportMoved;

    QString indexToString(int index);
    void updateViewportContentWidth();
    void updateViewportContentHeight();

    void requestTableItemAsync(int index);
    void deliverPostedTableItems();
    FxTableItemSG *getTableItem(int index);

    FxTableItemSG *visibleTableItem(int modelIndex) const;
    void showTableItem(FxTableItemSG *fxViewItem);

    bool canHaveMoreItemsInDirection(const FxTableItemSG *fxTableItem, const QPoint &direction) const;
    FxViewItem *itemNextTo(const FxTableItemSG *fxViewItem, const QPoint &direction) const;
    FxViewItem *tableEdgeItem(const FxTableItemSG *fxTableItem, Qt::Orientation orientation) const;

    qreal calculateTablePositionX(const FxTableItemSG *fxTableItem) const;
    qreal calculateTablePositionY(const FxTableItemSG *fxTableItem) const;
    void positionTableItem(FxTableItemSG *fxTableItem);

    void reloadTable(const QRectF &viewportRect);
    void refillItemsAlongEdges(const QRectF viewportRect);
    void refillItemsInDirection(const QPoint &startCell, const QPoint &fillDirection);
    void removeItemsAlongEdges(const QRectF viewportRect);
    void releaseItems(int fromColumn, int toColumn, int fromRow, int toRow);

    void enqueueLoadRequest(const TableSectionLoadRequest &request);
    void dequeueCurrentLoadRequest();
    void executeNextLoadRequest();
    void continueExecutingCurrentLoadRequest(const FxTableItemSG *receivedTableItem);
    void checkLoadRequestStatus();
    void tableItemLoaded(int index);

    void updateTableMetrics(int addedIndex, int removedIndex);
};

QQuickTableViewPrivate::QQuickTableViewPrivate()
    : layoutState(Idle)
    , currentLayoutRect(QRect())
    , currentTopLeftIndex(kNullValue)
    , currentTopRightColumn(kNullValue)
    , currentBottomLeftRow(kNullValue)
    , rowCount(-1)
    , columnCount(-1)
    , orientation(QQuickTableView::Vertical)
    , rowSpacing(0)
    , columnSpacing(0)
    , loadRequests(QQueue<TableSectionLoadRequest>())
    , eventTypeDeliverPostedTableItems(static_cast<QEvent::Type>(QEvent::registerEventType()))
    , postedTableItems(QVector<int>())
    , blockCreatedItemsSyncCallback(false)
    , forceSynchronousMode(qEnvironmentVariable("QT_TABLEVIEW_SYNC_MODE") == QLatin1String("true"))
    , inViewportMoved(false)
{
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

int QQuickTableViewPrivate::indexAt(const QPoint& cellCoord) const
{
    // NB: indexAt expects row first, which is y
    return indexAt(cellCoord.y(), cellCoord.x());
}

QPoint QQuickTableViewPrivate::cellCoordAt(int index) const
{
    return QPoint(columnAtIndex(index), rowAtIndex(index));
}

int QQuickTableViewPrivate::indexAt(int row, int column) const
{
    Q_Q(const QQuickTableView);
    int rows = q->rows();
    int columns = q->columns();
    if (row < 0 || row >= rows || column < 0 || column >= columns)
        return -1;
    return column + row * columns;
}

FxViewItem *QQuickTableViewPrivate::visibleItemAt(int row, int column) const
{
    return visibleItem(indexAt(row, column));
}

void QQuickTableViewPrivate::updateViewportContentWidth()
{
    Q_Q(QQuickTableView);
//    qreal contentWidth = 0;
//    for (int i = 0; i < columns; ++i)
//        contentWidth += columnWidth(i);
//    contentWidth += (columns - 1) * columnSpacing;
//    q->setContentWidth(contentWidth);

    qCDebug(lcItemViewDelegateLifecycle) << "TODO, change how we update width/height";
    q->setContentWidth(1000);
}

void QQuickTableViewPrivate::updateViewportContentHeight()
{
    Q_Q(QQuickTableView);
//    qreal contentHeight = 0;
//    for (int i = 0; i < rows; ++i)
//        contentHeight += rowHeight(i);
//    contentHeight += (rows - 1) * columnSpacing;
//    q->setContentHeight(contentHeight);

    q->setContentHeight(1000);
}

void QQuickTableView::viewportMoved(Qt::Orientations orient)
{
    Q_D(QQuickTableView);
    QQuickAbstractItemView::viewportMoved(orient);

    // Recursion can occur due to refill changing the content size.
    if (d->inViewportMoved)
        return;
    d->inViewportMoved = true;

    d->addRemoveVisibleItems();

    d->inViewportMoved = false;
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

QString QQuickTableViewPrivate::indexToString(int index)
{
    if (index == kNullValue)
        return QLatin1String("null index");
    return QString::fromLatin1("index: %1 (%2, %3)").arg(index).arg(rowAtIndex(index)).arg(columnAtIndex(index));
}

void QQuickTableViewPrivate::requestTableItemAsync(int index)
{
    qCDebug(lcItemViewDelegateLifecycle) << indexToString(index);

    // TODO: check if item is already available before creating an FxTableItemSG
    // so we don't have to release it below in case it exists.

    QBoolBlocker guard(blockCreatedItemsSyncCallback);
    FxTableItemSG *item = static_cast<FxTableItemSG *>(createItem(index, !forceSynchronousMode));

    if (item) {
        // This can happen if the item was cached by the model from before. But we really don't
        // want to check for this case everywhere, so we create an async callback anyway.
        // To avoid sending multiple events for different items, we take care to only
        // schedule one event, and instead queue the indices and process them
        // all in one go once we receive the event.
        qCDebug(lcItemViewDelegateLifecycle) << "item already loaded, posting it:" << indexToString(index);
        bool alreadyWaitingForPendingEvent = !postedTableItems.isEmpty();
        postedTableItems.append(index);

        if (!alreadyWaitingForPendingEvent && !forceSynchronousMode) {
            qCDebug(lcItemViewDelegateLifecycle) << "posting eventTypeDeliverPostedTableItems";
            QEvent *event = new QEvent(eventTypeDeliverPostedTableItems);
            qApp->postEvent(q_func(), event);
        }

        releaseItem(item);
    }
}

void QQuickTableViewPrivate::deliverPostedTableItems()
{
    qCDebug(lcItemViewDelegateLifecycle) << "initial count:" << postedTableItems.count();

    // When we deliver items, the receivers will typically ask for
    // not only the item we deliver, but also subsequent items in
    // the layout. To avoid recursion, we need to block.
    QBoolBlocker guard(blockCreatedItemsSyncCallback);

    // Note that as we deliver items from this function, new items
    // might be appended to the list, which means that we can end
    // up delivering more items than what we had when we started.
    for (int i = 0; i < postedTableItems.count(); ++i) {
        const int index = postedTableItems[i];
        qCDebug(lcItemViewDelegateLifecycle) << "deliver:" << index;
        tableItemLoaded(index);
    }

    postedTableItems.clear();

    qCDebug(lcItemViewDelegateLifecycle) << "done delivering items!";
}

FxTableItemSG *QQuickTableViewPrivate::getTableItem(int index)
{
    // This function should be called whenever we receive an
    // item async to ensure that we ref-count the item.
    FxTableItemSG *item = static_cast<FxTableItemSG *>(createItem(index, false));

    // The item was requested sync, so it should be ready!
    Q_ASSERT(item);
    Q_ASSERT(item->item);

    return item;
}

void QQuickTableViewPrivate::releaseItems(int fromColumn, int toColumn, int fromRow, int toRow)
{
    qCDebug(lcItemViewDelegateLifecycle) << "release items ( x1:"
                                         << fromColumn << ", x2:" << toColumn << ", y1:"
                                         << fromRow << ", y2:" << toRow  << ")";

    for (int row = fromRow; row <= toRow; ++row) {
        for (int col = fromColumn; col <= toColumn; ++col) {
            FxViewItem *item = visibleItemAt(row, col);
            visibleItems.removeOne(item);
            releaseItem(item);
        }
    }
}

FxTableItemSG *QQuickTableViewPrivate::visibleTableItem(int modelIndex) const
{
    // TODO: this is an overload of visibleItems, since the other version
    // does some wierd things with the index. Fix that up later. And change
    // visibleItems to use hash or map instead of list.
    for (int i = 0; i < visibleItems.count(); ++i) {
        FxViewItem *item = visibleItems.at(i);
        if (item->index == modelIndex)
            return static_cast<FxTableItemSG *>(item);
    }
    return 0;
}

void QQuickTableViewPrivate::showTableItem(FxTableItemSG *fxViewItem)
{
    // Add the item to the list of visible items, and show it
    visibleItems.append(fxViewItem);
    QQuickItemPrivate::get(fxViewItem->item)->setCulled(false);
}

FxViewItem *QQuickTableViewPrivate::itemNextTo(const FxTableItemSG *fxViewItem, const QPoint &direction) const
{
    int index = fxViewItem->index;
    int row = rowAtIndex(index) + direction.y();
    int column = columnAtIndex(index) + direction.x();
    return visibleTableItem(indexAt(row, column));
}

FxViewItem *QQuickTableViewPrivate::tableEdgeItem(const FxTableItemSG *fxTableItem, Qt::Orientation orientation) const
{
    int index = fxTableItem->index;

    if (orientation == Qt::Horizontal) {
        int row = rowAtIndex(index);
        int edgeColumn = columnAtIndex(currentTopLeftIndex);
        return visibleTableItem(indexAt(row, edgeColumn));
    } else {
        int column = columnAtIndex(index);
        int edgeRow = rowAtIndex(currentTopLeftIndex);
        return visibleTableItem(indexAt(edgeRow, column));
    }
}

bool QQuickTableViewPrivate::canHaveMoreItemsInDirection(const FxTableItemSG *fxTableItem, const QPoint &direction) const
{
    int row = rowAtIndex(fxTableItem->index);
    int column = columnAtIndex(fxTableItem->index);
    QQuickItem *tableItemItem = fxTableItem->item;
    QRectF itemRect = QRectF(tableItemItem->position(), tableItemItem->size());

    if (direction.x() == 1) {
        if (column > columnCount)
            return false;
        return itemRect.topRight().x() < currentLayoutRect.topRight().x();
    } else if (direction.x() == -1) {
        if (column == 0)
            return false;
        return itemRect.topLeft().x() > currentLayoutRect.topLeft().x();
    } else if (direction.y() == 1) {
        if (row > rowCount)
            return false;
        return itemRect.bottomLeft().y() < currentLayoutRect.bottomLeft().y();
    } else if (direction.y() == -1) {
        if (row == 0)
            return false;
        return itemRect.topLeft().y() > currentLayoutRect.topLeft().y();
    } else {
        Q_UNREACHABLE();
    }

    return false;
}

qreal QQuickTableViewPrivate::calculateTablePositionX(const FxTableItemSG *fxTableItem) const
{
    // Calculate the table position x of fxViewItem based on the position of the item on
    // the horizontal edge, or otherwise the item directly next to it. This assumes that at
    // least the item next to it has already been loaded. If the edge item has been
    // loaded, we use that as reference instead, as it allows us to load several items in parallel.

    if (fxTableItem->index == currentTopLeftIndex) {
        // Special case: the top left item has pos 0, 0 for now
        return 0;
    }

    bool isEdgeItem = rowAtIndex(fxTableItem->index) == rowAtIndex(currentTopLeftIndex);

    if (isEdgeItem) {
        FxViewItem *fxViewAnchorItem = itemNextTo(fxTableItem, QPoint(-1, 0));
        Q_ASSERT(fxViewAnchorItem);
        QQuickItem *anchorItem = fxViewAnchorItem->item;
        return anchorItem->position().x() + anchorItem->size().width() + columnSpacing;
    } else {
        FxViewItem *fxViewAnchorItem = tableEdgeItem(fxTableItem, Qt::Vertical);
        Q_ASSERT(fxViewAnchorItem);
        QQuickItem *anchorItem = fxViewAnchorItem->item;
        return anchorItem->position().x();
    }
}

qreal QQuickTableViewPrivate::calculateTablePositionY(const FxTableItemSG *fxTableItem) const
{
    // Calculate the table position y of fxViewItem based on the position of the item on
    // the vertical edge, or otherwise the item directly next to it. This assumes that at
    // least the item next to it has already been loaded. If the edge item has been
    // loaded, we use that as reference instead, as it allows us to load several items in parallel.

    if (fxTableItem->index == currentTopLeftIndex) {
        // Special case: the top left item has pos 0, 0 for now
        return 0;
    }

    bool isEdgeItem = columnAtIndex(fxTableItem->index) == columnAtIndex(currentTopLeftIndex);

    if (isEdgeItem) {
        FxViewItem *fxViewAnchorItem = itemNextTo(fxTableItem, QPoint(0, -1));
        Q_ASSERT(fxViewAnchorItem);
        QQuickItem *anchorItem = fxViewAnchorItem->item;
        return anchorItem->position().y() + anchorItem->size().height() + rowSpacing;
    } else {
        FxViewItem *fxViewAnchorItem = tableEdgeItem(fxTableItem, Qt::Horizontal);
        Q_ASSERT(fxViewAnchorItem);
        QQuickItem *anchorItem = fxViewAnchorItem->item;
        return anchorItem->position().y();
    }
}

void QQuickTableViewPrivate::positionTableItem(FxTableItemSG *fxTableItem)
{
    int x = calculateTablePositionX(fxTableItem);
    int y = calculateTablePositionY(fxTableItem);
    fxTableItem->setPosition(QPointF(x, y));
}

void QQuickTableViewPrivate::updateTableMetrics(int addedIndex, int removedIndex)
{
    int row = rowAtIndex(addedIndex);
    int column = columnAtIndex(addedIndex);
    currentTopRightColumn = qMax(column, currentTopRightColumn);
    currentBottomLeftRow = qMax(row, currentBottomLeftRow);

    // When all edge items of the table has been loaded, we have enough
    // information to calculate column widths and row height etc.
//    int rowCount = currentLayoutRequest.visualRowCount;
//    int columnCount = currentLayoutRequest.visualColumnCount;
//    int topLeftRow = rowAtIndex(currentTopLeftIndex);
//    int topLeftColumn = columnAtIndex(currentTopLeftIndex);

//    currentLayoutRequest.topRightIndex = indexAt(topLeftRow, topLeftColumn + columnCount - 1);
//    currentLayoutRequest.bottomLeftIndex = indexAt(topLeftRow + rowCount - 1, topLeftColumn);
//    currentLayoutRequest.bottomRightIndex = indexAt(topLeftRow + rowCount - 1, topLeftColumn + columnCount - 1);

//    qCDebug(lcItemViewDelegateLifecycle) << "row count" << rowCount << "column count:" << columnCount;
//    qCDebug(lcItemViewDelegateLifecycle) << "top left:" << indexToString(currentTopLeftIndex);
//    qCDebug(lcItemViewDelegateLifecycle) << "bottom left:" << indexToString(currentLayoutRequest.bottomLeftIndex);
//    qCDebug(lcItemViewDelegateLifecycle) << "top right:" << indexToString(currentLayoutRequest.topRightIndex);
//    qCDebug(lcItemViewDelegateLifecycle) << "bottom right:" << indexToString(currentLayoutRequest.bottomRightIndex);
}

void QQuickTableView::createdItem(int index, QObject* object)
{
    Q_D(QQuickTableView);

    if (index == d->requestedIndex) {
        // This is needed by the model.
        // TODO: let model have it's own handler, and refactor requestedIndex out the view?
        d->requestedIndex = -1;
    }

    Q_ASSERT(d->layoutState != QQuickTableViewPrivate::Idle);

    if (d->blockCreatedItemsSyncCallback) {
        // Items that are created synchronously will still need to
        // be delivered async later, to funnel all item creation through
        // the same async layout logic.
        return;
    }

    Q_ASSERT(qmlobject_cast<QQuickItem*>(object));
    qCDebug(lcItemViewDelegateLifecycle) << "deliver:" << index;
    d->tableItemLoaded(index);
}

void QQuickTableViewPrivate::tableItemLoaded(int index)
{
    qCDebug(lcItemViewDelegateLifecycle) << indexToString(index);

    FxTableItemSG *receivedTableItem = getTableItem(index);
    positionTableItem(receivedTableItem);
    showTableItem(receivedTableItem);
    updateTableMetrics(index, kNullValue);
    continueExecutingCurrentLoadRequest(receivedTableItem);
    checkLoadRequestStatus();
}

void QQuickTableViewPrivate::checkLoadRequestStatus()
{
    if (!loadRequests.head().done)
        return;

    dequeueCurrentLoadRequest();

    if (!loadRequests.isEmpty()) {
        executeNextLoadRequest();
    } else {
        // Nothing more todo. Check if the view port moved while we
        // were processing load requests, and if so, start all over.
        layoutState = Idle;
        addRemoveVisibleItems();
    }
}

void QQuickTableViewPrivate::enqueueLoadRequest(const TableSectionLoadRequest &request)
{
    loadRequests.enqueue(request);
    qCDebug(lcItemViewDelegateLifecycle) << request;
}

void QQuickTableViewPrivate::dequeueCurrentLoadRequest()
{
    const TableSectionLoadRequest request = loadRequests.dequeue();
    qCDebug(lcItemViewDelegateLifecycle) << request;
}

void QQuickTableViewPrivate::executeNextLoadRequest()
{
    Q_ASSERT(!loadRequests.isEmpty());
    layoutState = ProcessingLoadRequest;

    TableSectionLoadRequest &request = loadRequests.head();
    qCDebug(lcItemViewDelegateLifecycle) << request;

    switch (request.loadMode) {
    case TableSectionLoadRequest::LoadOneByOne:
        requestTableItemAsync(indexAt(request.startCell));
        break;
    case TableSectionLoadRequest::LoadInParallel: {
        // Note: TableSectionLoadRequest::LoadInParallel can only work when we
        // know the column widths and row heights of the items we try to load.
        // And this information is found by looking at the table edge items. So
        // those items must be loaded first. And when doing so, we determine
        // the current table metrics on the way, like visual row count/column
        // count, which we then use below to figure out the number of items to request.
        int topLeftRow = rowAtIndex(currentTopLeftIndex);
        int topLeftColumn = columnAtIndex(currentTopLeftIndex);
        int rows = currentBottomLeftRow - topLeftRow + 1;
        int columns = currentTopRightColumn - topLeftColumn + 1;

        for (int y = request.startCell.y(); y < rows; ++y) {
            for (int x = request.startCell.x(); x < columns; ++x) {
                request.requestedItemCount++;
                int index = indexAt(topLeftRow + y, topLeftColumn + x);
                requestTableItemAsync(index);
            }
        }
        break; }
    }
}

void QQuickTableViewPrivate::continueExecutingCurrentLoadRequest(const FxTableItemSG *receivedTableItem)
{
    Q_ASSERT(!loadRequests.isEmpty());
    TableSectionLoadRequest &request = loadRequests.head();
    qCDebug(lcItemViewDelegateLifecycle()) << request;

    if (request.onlyOneItemRequested()) {
        request.done = true;
        return;
    }

    switch (request.loadMode) {
    case TableSectionLoadRequest::LoadOneByOne: {
        request.done = !canHaveMoreItemsInDirection(receivedTableItem, request.fillDirection);
        if (!request.done) {
            const QPoint nextCell = cellCoordAt(receivedTableItem->index) + request.fillDirection;
            requestTableItemAsync(indexAt(nextCell));
        }
        break; }
    case TableSectionLoadRequest::LoadInParallel:
        // All items have already been requested. Just count them in.
        request.done = --request.requestedItemCount == 0;
        break;
    }
}

void QQuickTableViewPrivate::reloadTable(const QRectF &viewportRect)
{
    qCDebug(lcItemViewDelegateLifecycle());

    // todo: cancel pending requested items!
    releaseVisibleItems();

    currentLayoutRect = viewportRect;
    currentTopLeftIndex = indexAt(0, 0);

    int topLeftRow = rowAtIndex(currentTopLeftIndex);
    int topLeftColumn = columnAtIndex(currentTopLeftIndex);

    TableSectionLoadRequest requestEdgeRow;
    requestEdgeRow.startCell = QPoint(topLeftColumn, topLeftRow);
    requestEdgeRow.fillDirection = QPoint(1, 0);
    requestEdgeRow.loadMode = TableSectionLoadRequest::LoadOneByOne;
    enqueueLoadRequest(requestEdgeRow);

    TableSectionLoadRequest requestEdgeColumn;
    requestEdgeColumn.startCell = QPoint(topLeftColumn, topLeftRow + 1);
    requestEdgeColumn.fillDirection = QPoint(0, 1);
    requestEdgeColumn.loadMode = TableSectionLoadRequest::LoadOneByOne;
    enqueueLoadRequest(requestEdgeColumn);

    TableSectionLoadRequest requestInnerItems;
    requestInnerItems.startCell = QPoint(topLeftColumn + 1, topLeftRow + 1);
    requestInnerItems.fillDirection = QPoint(1, 1);
    requestInnerItems.loadMode = TableSectionLoadRequest::LoadInParallel;
    enqueueLoadRequest(requestInnerItems);
}

void QQuickTableViewPrivate::removeItemsAlongEdges(const QRectF viewportRect)
{
    Q_UNIMPLEMENTED();
}

void QQuickTableViewPrivate::refillItemsAlongEdges(const QRectF viewportRect)
{
    currentLayoutRect = viewportRect;

    int topLeftRow = rowAtIndex(currentTopLeftIndex);
    int topLeftColumn = columnAtIndex(currentTopLeftIndex);
    FxTableItemSG *topRightItem = visibleTableItem(indexAt(topLeftRow, currentTopRightColumn));
    FxTableItemSG *bottomLeftItem = visibleTableItem(indexAt(currentBottomLeftRow, topLeftColumn));

    const QPoint left = QPoint(-1, 0);
    const QPoint right = QPoint(1, 0);
    const QPoint up = QPoint(0, -1);
    const QPoint down = QPoint(0, 1);

    if (canHaveMoreItemsInDirection(bottomLeftItem, left)) {
        QPoint startCell = cellCoordAt(bottomLeftItem->index) + left;
        refillItemsInDirection(startCell, up);
    } else if (canHaveMoreItemsInDirection(topRightItem, right)) {
        QPoint startCell = cellCoordAt(topRightItem->index) + right;
        refillItemsInDirection(startCell, down);
    }

    if (canHaveMoreItemsInDirection(topRightItem, up)) {
        QPoint startCell = cellCoordAt(topRightItem->index) + up;
        refillItemsInDirection(startCell, left);
    } else if (canHaveMoreItemsInDirection(bottomLeftItem, down)) {
        QPoint startCell = cellCoordAt(bottomLeftItem->index) + down;
        refillItemsInDirection(startCell, right);
    }
}

void QQuickTableViewPrivate::refillItemsInDirection(const QPoint &startCell, const QPoint &fillDirection)
{
    TableSectionLoadRequest edgeItemRequest;
    edgeItemRequest.startCell = startCell;
    enqueueLoadRequest(edgeItemRequest);

    TableSectionLoadRequest trailingItemsRequest;
    trailingItemsRequest.startCell = startCell + fillDirection;
    trailingItemsRequest.fillDirection = fillDirection;
    trailingItemsRequest.loadMode = TableSectionLoadRequest::LoadInParallel;
    enqueueLoadRequest(trailingItemsRequest);
}

bool QQuickTableViewPrivate::addRemoveVisibleItems()
{
    Q_Q(QQuickTableView);

    if (layoutState != Idle)
        return false;

    QRectF viewportRect = QRectF(QPointF(q->contentX(), q->contentY()), q->size());
    if (!viewportRect.isValid())
        return false;

    if (visibleItems.isEmpty()) {
        // Do a complete refill from scratch
        reloadTable(viewportRect);
    } else {
        // Refill items around the already visible table items
        removeItemsAlongEdges(viewportRect);
        refillItemsAlongEdges(viewportRect);
    }

    if (!loadRequests.isEmpty()) {
        executeNextLoadRequest();

        if (forceSynchronousMode)
            deliverPostedTableItems();
    }

    // return false? or override caller? Or check load queue?
    return true;
}

QQuickTableView::QQuickTableView(QQuickItem *parent)
    : QQuickAbstractItemView(*(new QQuickTableViewPrivate), parent)
{
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

    d->updateViewportContentHeight();

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

    d->updateViewportContentWidth();

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

bool QQuickTableView::event(QEvent *e)
{
    Q_D(QQuickTableView);

    if (e->type() == d->eventTypeDeliverPostedTableItems) {
        d->deliverPostedTableItems();
        return true;
    }

    return QQuickAbstractItemView::event(e);
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

    // NB: deliberatly skipping QQuickAbstractItemView, since it does so
    // many wierd things that I don't need or understand. That code is messy...
    QQuickFlickable::componentComplete();
}

QT_END_NAMESPACE
