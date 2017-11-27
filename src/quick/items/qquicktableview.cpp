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

    int startRow = kNullValue;
    int startColumn = kNullValue;
    Qt::Edges fillDirection;
    LoadMode loadMode;
};

QDebug operator<<(QDebug dbg, const TableSectionLoadRequest request) {
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "TableSectionLoadRequest(";
    dbg << "start:[" << request.startRow << ", " << request.startColumn << "]";
    dbg << " direction:" << request.fillDirection;
    dbg << " mode:" << request.loadMode;
    dbg << ')';
    return dbg;
}

class GridLayoutRequest
{
public:
    enum LayoutState {
        Idle,
        LoadingSelectedItem,
        LoadingTableRequest,
    };

    GridLayoutRequest(const QRectF &viewportRect)
        : state(Idle)
        , viewportRect(viewportRect)
        , pendingViewportRect(QRectF())
        , topLeftIndex(kNullValue)
        , bottomLeftRow(kNullValue)
        , topRightColumn(kNullValue)
        , requestedTopEdgeIndex(kNullValue)
        , requestedLeftEdgeIndex(kNullValue)
        , requestedItemCount(0)
    {}

    LayoutState state;
    QRectF viewportRect;
    QRectF pendingViewportRect;

    int topLeftIndex;
    int bottomLeftRow;
    int topRightColumn;

    int requestedTopEdgeIndex;
    int requestedLeftEdgeIndex;
    int requestedItemCount;
};

class QQuickTableViewPrivate : public QQuickAbstractItemViewPrivate
{
    Q_DECLARE_PUBLIC(QQuickTableView)

public:

    QQuickTableViewPrivate();

    bool isRightToLeft() const;
    bool isBottomToTop() const;

    int rowAtIndex(int index) const;
    int rowAtPos(qreal y) const;
    int columnAtIndex(int index) const;
    int columnAtPos(qreal x) const;
    int indexAt(int row, int column) const;
    FxViewItem *visibleItemAt(int row, int column) const;

    qreal rowPos(int row) const;
    qreal rowHeight(int row) const;
    qreal columnPos(int column) const;
    qreal columnWidth(int column) const;

    QSizeF size() const;
    QPointF position() const;

    QPointF cellPosition(int row, int column) const;
    QPointF cellPosition(FxViewItem *item) const;
    QPointF itemEndPosition(FxViewItem *item) const;
    QSizeF itemSize(FxViewItem *item) const;

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
    int rows;
    int columns;
    int visibleRows;
    int visibleColumns;
    QQuickTableView::Orientation orientation;
    qreal rowSpacing;
    qreal columnSpacing;
    QPointF prevContentPos;
    QSizeF prevViewSize;
    GridLayoutRequest currentLayoutRequest;
    QEvent::Type eventTypeDeliverTableItems;
    QVector<int> deliverTableItemsIndexList;
    bool blockCreatedItemsSyncCallback;
    bool forceSynchronousMode;
    QQueue<TableSectionLoadRequest> loadRequests;

    QMargins prevGrid;
    mutable QPair<int, qreal> rowPositionCache;
    mutable QPair<int, qreal> columnPositionCache;
    QVector<qreal> rowHeightCache_unused;
    QVector<qreal> columnWidthCache_unused;

    QHash<int, qreal> columnWidthCache;
    QHash<int, qreal> rowHeightCache;

    bool inViewportMoved;

    void updateViewportContentWidth();
    void updateViewportContentHeight();

    void requestTableItemAsync(int index);
    void deliverPostedTableItems();
    FxTableItemSG *getTableItem(int index);
    FxTableItemSG *createTableItem(int index, QQmlIncubator::IncubationMode mode);

    void createTableItems(int fromColumn, int toColumn, int fromRow, int toRow);
    void releaseItems(int fromColumn, int toColumn, int fromRow, int toRow);
    bool removeItemsOutsideView(const QMargins &previousGrid, const QMargins &currentGrid);
    bool addItemsInsideView(const QMargins &previousGrid, const QMargins &currentGrid);
    qreal getCachedRowHeight(int row);
    qreal getCachedColumnWidth(int col);
    void fillTable();

    FxTableItemSG *visibleTableItem(int modelIndex) const;
    void showTableItem(FxTableItemSG *fxViewItem);

    void checkForPendingRequests();

    QString indexToString(int index);
    bool canHaveMoreItemsBelow(FxTableItemSG *fxViewItem);
    bool canHaveMoreItemsRightOf(FxTableItemSG *fxViewItem);

    FxViewItem *itemLeftOf(FxTableItemSG *fxViewItem);
    FxViewItem *itemOnTopOf(FxTableItemSG *fxViewItem);
    FxViewItem *itemOnTopEdgeOf(FxTableItemSG *fxViewItem);
    FxViewItem *itemOnLeftEdgeOf(FxTableItemSG *fxViewItem);

    qreal calculateTablePositionX(FxTableItemSG *fxViewItem);
    qreal calculateTablePositionY(FxTableItemSG *fxViewItem);

    void reloadTable(const QRectF &viewportRect);

    void enqueueLoadRequest(const TableSectionLoadRequest &request);
    void dequeueCurrentLoadRequest();
    void executeNextLoadRequest();
    void tableItemLoaded(int index);

    void bookkeepRequest(const QRectF viewportRect);
    void addVisibleItems(const QRectF viewportRect);
    void calculateTableGridMetrics();
    void requestNextLeftEdgeItem(FxTableItemSG *fxTableItem);
    void requestNextTopEdgeItem(FxTableItemSG *fxTableItem);
};

QQuickTableViewPrivate::QQuickTableViewPrivate()
    : rows(-1)
    , columns(-1)
    , visibleRows(0)
    , visibleColumns(0)
    , orientation(QQuickTableView::Vertical)
    , rowSpacing(0)
    , columnSpacing(0)
    , prevContentPos(QPointF(0, 0))
    , prevViewSize(QSizeF(0, 0))
    , currentLayoutRequest(QRect())
    , eventTypeDeliverTableItems(static_cast<QEvent::Type>(QEvent::registerEventType()))
    , deliverTableItemsIndexList(QVector<int>())
    , blockCreatedItemsSyncCallback(false)
    , forceSynchronousMode(false)
    , loadRequests(QQueue<TableSectionLoadRequest>())
    , prevGrid(QMargins(-1, -1, -1, -1))
    , rowPositionCache(qMakePair(0, 0))
    , columnPositionCache(qMakePair(0, 0))
    , rowHeightCache_unused(QVector<qreal>(100, kNullValue))
    , columnWidthCache_unused(QVector<qreal>(100, kNullValue))
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

int QQuickTableViewPrivate::rowAtPos(qreal y) const
{
    Q_Q(const QQuickTableView);

    // We cache the look-up, since it is likely that future
    // rowPos/rowAtPos calls will be for rows adjacent to
    // (or close to) the one requested. That way, we usually need to iterate
    // over just one row to get the correct column, at least while flicking.

    if (y <= 0) {
        rowPositionCache.first = 0;
        rowPositionCache.second = 0;
        return 0;
    } else if (y >= q->contentHeight()) {
        int lastRow = rows - 1;
        qreal lastRowPos = q->contentHeight() - rowHeight(lastRow);
        rowPositionCache.first = lastRow;
        rowPositionCache.second = lastRowPos;
        return lastRow;
    }

    const int cachedRow = rowPositionCache.first;
    const qreal cachedRowPos = rowPositionCache.second;

    int row = cachedRow;
    qreal rowPos = cachedRowPos;

    if (y > cachedRowPos) {
        for (row = cachedRow; row < rows; ++row) {
            qreal height = rowHeight(row) + rowSpacing;
            if (y < rowPos + height)
                break;
            rowPos += height;
        }
    } else if (y < cachedRowPos) {
        for (row = cachedRow - 1; row >= 0; --row) {
            rowPos -= rowHeight(row) + rowSpacing;
            if (y >= rowPos)
                break;
        }
    }

    rowPositionCache.first = row;
    rowPositionCache.second = rowPos;

    return row;
}

qreal QQuickTableViewPrivate::rowPos(int row) const
{
    Q_Q(const QQuickTableView);

    // We cache the look-up, since it is likely that future
    // rowPos/rowAtPos calls will be for rows adjacent to
    // (or close to) the one requested. That way, we usually need to iterate
    // over just one row to get the correct position, at least while flicking.

    const int cachedRow = rowPositionCache.first;
    const qreal cachedRowPos = rowPositionCache.second;

    if (row == cachedRow)
        return cachedRowPos;

    if (row <= 0) {
        rowPositionCache.first = 0;
        rowPositionCache.second = 0;
        return 0;
    } else if (row >= rows) {
        int lastRow = rows - 1;
        qreal lastRowPos = q->contentHeight() - rowHeight(lastRow);
        rowPositionCache.first = lastRow;
        rowPositionCache.second = lastRowPos;
        return lastRowPos;
    }

    qreal rowPos = cachedRowPos;

    if (row > cachedRow) {
        for (int i = cachedRow; i < row; ++i)
            rowPos += rowHeight(i) + rowSpacing;
    } else {
        for (int i = cachedRow - 1; i >= row; --i)
            rowPos -= rowHeight(i) + rowSpacing;
    }

    rowPositionCache.first = row;
    rowPositionCache.second = rowPos;

    return rowPos;
}

qreal QQuickTableViewPrivate::rowHeight(int row) const
{
    int rowHeight = rowHeightCache_unused[row];
    if (rowHeight != kNullValue)
        return rowHeight;

    qWarning() << "row height not found in cache:" << row;
    return kNullValue;

//    srand(row);
//    int r = ((rand() % 40) * 2) + 20;
//    return r;

//    int column = columnAt(visibleIndex);
//    FxViewItem *item = visibleItemAt(row, column);
//    return item ? item->itemHeight() : averageSize.height();
}

int QQuickTableViewPrivate::columnAtPos(qreal x) const
{
    Q_Q(const QQuickTableView);

    // We cache the look-up, since it is likely that future
    // columnPos/columnAtPos calls will be for columns adjacent to
    // (or close to) the one requested. That way, we usually need to iterate
    // over just one column to get the correct column, at least while flicking.

    if (x <= 0) {
        columnPositionCache.first = 0;
        columnPositionCache.second = 0;
        return 0;
    } else if (x >= q->contentWidth()) {
        int lastColumn = columns - 1;
        qreal lastColumnPos = q->contentWidth() - columnWidth(lastColumn);
        columnPositionCache.first = lastColumn;
        columnPositionCache.second = lastColumnPos;
        return lastColumn;
    }

    const int cachedColumn = columnPositionCache.first;
    const qreal cachedColumnPos = columnPositionCache.second;

    int column = cachedColumn;
    qreal columnPos = cachedColumnPos;

    if (x > cachedColumnPos) {
        for (column = cachedColumn; column < columns; ++column) {
            qreal width = columnWidth(column) + columnSpacing;
            if (x < columnPos + width)
                break;
            columnPos += width;
        }
    } else if (x < cachedColumnPos) {
        for (column = cachedColumn - 1; column >= 0; --column) {
            columnPos -= columnWidth(column) + columnSpacing;
            if (x >= columnPos)
                break;
        }
    }

    columnPositionCache.first = column;
    columnPositionCache.second = columnPos;

    return column;
}

qreal QQuickTableViewPrivate::columnPos(int column) const
{
    Q_Q(const QQuickTableView);

    // We cache the look-up, since it is likely that future
    // columnPos/columnAtPos calls will be for columns adjacent to
    // (or close to) the one requested. That way, we usually need to iterate
    // over just one column to get the correct position, at least while flicking.

    const int cachedColumn = columnPositionCache.first;
    const qreal cachedColumnPos = columnPositionCache.second;

    if (column == cachedColumn)
        return cachedColumnPos;

    if (column <= 0) {
        columnPositionCache.first = 0;
        columnPositionCache.second = 0;
        return 0;
    } else if (column >= columns) {
        int lastColumn = columns - 1;
        qreal lastColumnPos = q->contentWidth() - columnWidth(lastColumn);
        columnPositionCache.first = lastColumn;
        columnPositionCache.second = lastColumnPos;
        return lastColumnPos;
    }

    qreal columnPos = cachedColumnPos;

    if (column > cachedColumn) {
        for (int i = cachedColumn; i < column; ++i)
            columnPos += columnWidth(i) + columnSpacing;
    } else {
        for (int i = cachedColumn - 1; i >= column; --i)
            columnPos -= columnWidth(i) + columnSpacing;
    }

    columnPositionCache.first = column;
    columnPositionCache.second = columnPos;

    return columnPos;
}

qreal QQuickTableViewPrivate::columnWidth(int column) const
{
    int columnWidth = columnWidthCache_unused[column];
    if (columnWidth != kNullValue)
        return columnWidth;

    qWarning() << "column width not found in cache:" << column;
    return kNullValue;

//    srand(column);
//    int r = ((rand() % 100) * 2) + 50;
//    return r;

//    int row = rowAt(visibleIndex);
//    FxViewItem *item = visibleItemAt(row, column);
//    return item ? item->itemWidth() : averageSize.width();
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

QSizeF QQuickTableViewPrivate::size() const
{
    Q_Q(const QQuickTableView);
    return orientation == QQuickTableView::Vertical ? QSizeF(q->width(), q->height()) : QSizeF(q->height(), q->width());
}

QPointF QQuickTableViewPrivate::position() const
{
    Q_Q(const QQuickTableView);
    return  orientation == QQuickTableView::Vertical ? QPointF(q->contentX(), q->contentY()) : QPointF(q->contentY(), q->contentX());
}

QPointF QQuickTableViewPrivate::cellPosition(int row, int column) const
{
   return QPointF(columnPos(column), rowPos(row));
}

QPointF QQuickTableViewPrivate::cellPosition(FxViewItem *item) const
{
    if (isContentFlowReversed())
        return QPointF(-item->itemX() - item->itemWidth(), -item->itemY() - item->itemHeight());
    else
        return QPointF(item->itemX(), item->itemY());
}

QPointF QQuickTableViewPrivate::itemEndPosition(FxViewItem *item) const
{
    if (isContentFlowReversed())
        return QPointF(-item->itemX(), -item->itemY());
    else
        return QPointF(item->itemX() + item->itemWidth(), item->itemY() + item->itemHeight());
}

QSizeF QQuickTableViewPrivate::itemSize(FxViewItem *item) const
{
    return QSizeF(item->itemWidth(), item->itemHeight());
}

static QPointF operator+(const QPointF &pos, const QSizeF &size)
{
    return QPointF(pos.x() + size.width(), pos.y() + size.height());
}

static QPointF operator-(const QPointF &pos, const QSizeF &size)
{
    return QPointF(pos.x() + size.width(), pos.y() + size.height());
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

void QQuickTableView::createdItem(int index, QObject* object)
{
    Q_D(QQuickTableView);

    if (index == d->requestedIndex) {
        // This is needed by the model. Essientially the same as GridLayoutRequest::waitingForIndex.
        // TODO: let model have it's own handler, and refactor requestedIndex out the view?
        d->requestedIndex = -1;
    }

    Q_ASSERT(d->currentLayoutRequest.state != GridLayoutRequest::Idle);

    if (d->blockCreatedItemsSyncCallback) {
        // Items that are created synchronously will still need to
        // be delivered async later, to funnel all event creation through
        // the same async layout logic.
        return;
    }

    Q_ASSERT(qmlobject_cast<QQuickItem*>(object));
    qCDebug(lcItemViewDelegateLifecycle) << "deliver:" << index;
    d->tableItemLoaded(index);
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

qreal QQuickTableViewPrivate::getCachedRowHeight(int row)
{
    return 20;
}

qreal QQuickTableViewPrivate::getCachedColumnWidth(int col)
{
    return 100;
}

QString QQuickTableViewPrivate::indexToString(int index)
{
    if (index == kNullValue)
        return QLatin1String("null index");
    return QString::fromLatin1("index: %1 (%2, %3)").arg(index).arg(rowAtIndex(index)).arg(columnAtIndex(index));
}

FxTableItemSG *QQuickTableViewPrivate::createTableItem(int index, QQmlIncubator::IncubationMode mode)
{
    // Todo: change this later. Not sure if we should always load async, or if it makes sense to
    // load items inside visibleContentView sync. This needs experimentation.
    bool loadAsync = (mode != QQmlIncubator::Synchronous);

    FxTableItemSG *item = static_cast<FxTableItemSG *>(createItem(index, loadAsync));

    if (!item) {
        // Item is not done incubating, or there is an error situation
        return nullptr;
    }

    if (!item->item) {
        qCDebug(lcItemViewDelegateLifecycle)
                << "failed to create QQuickItem for index/row/col:"
                << index << rowAtIndex(index) << columnAtIndex(index);
        releaseItem(item);
        return nullptr;
    }

    return item;
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
        qCDebug(lcItemViewDelegateLifecycle) << "item already loaded!" << deliverTableItemsIndexList.count();
        bool alreadyWaitingForPendingEvent = !deliverTableItemsIndexList.isEmpty();
        deliverTableItemsIndexList.append(index);

        if (!alreadyWaitingForPendingEvent) {
            qCDebug(lcItemViewDelegateLifecycle) << "posting eventTypeDeliverTableItems";
            QEvent *event = new QEvent(eventTypeDeliverTableItems);
            qApp->postEvent(q_func(), event);
        }

        releaseItem(item);
    }
}

void QQuickTableViewPrivate::deliverPostedTableItems()
{
    qCDebug(lcItemViewDelegateLifecycle) << "initial count:" << deliverTableItemsIndexList.count();

    // When we deliver items, the receivers will typically ask for
    // not only the item we deliver, but also subsequent items in
    // the layout. To avoid recursion, we need to block.
    QBoolBlocker guard(blockCreatedItemsSyncCallback);

    // Note that as we deliver items from this function, new items
    // might be appended to the list, which means that we might end
    // up delivering more items than what we got before we started.
    for (int i = 0; i < deliverTableItemsIndexList.count(); ++i) {
        const int index = deliverTableItemsIndexList[i];
        qCDebug(lcItemViewDelegateLifecycle) << "deliver:" << index;
        tableItemLoaded(index);
    }

    deliverTableItemsIndexList.clear();

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

FxViewItem *QQuickTableViewPrivate::itemLeftOf(FxTableItemSG *fxViewItem)
{
    int index = fxViewItem->index;
    int row = rowAtIndex(index);
    int columnOnTheLeft = columnAtIndex(index) - 1;
    int indexOnTheLeft = indexAt(row, columnOnTheLeft);
    return visibleTableItem(indexOnTheLeft);
}

FxViewItem *QQuickTableViewPrivate::itemOnTopOf(FxTableItemSG *fxViewItem)
{
    int index = fxViewItem->index;
    int column = columnAtIndex(index);
    int rowAbove = rowAtIndex(index) - 1;
    int indexAbove = indexAt(rowAbove, column);
    return visibleTableItem(indexAbove);
}

FxViewItem *QQuickTableViewPrivate::itemOnTopEdgeOf(FxTableItemSG *fxViewItem)
{
    int index = fxViewItem->index;
    int column = columnAtIndex(index);
    int edgeRow = rowAtIndex(currentLayoutRequest.topLeftIndex);
    int edgeRowIndex = indexAt(edgeRow, column);
    return visibleTableItem(edgeRowIndex);
}

FxViewItem *QQuickTableViewPrivate::itemOnLeftEdgeOf(FxTableItemSG *fxViewItem)
{
    int index = fxViewItem->index;
    int row = rowAtIndex(index);
    int edgeColumn = columnAtIndex(currentLayoutRequest.topLeftIndex);
    int edgeColumnIndex = indexAt(row, edgeColumn);
    return visibleTableItem(edgeColumnIndex);
}

qreal QQuickTableViewPrivate::calculateTablePositionX(FxTableItemSG *fxViewItem)
{
    // Calculate the table position x of fxViewItem based on the position of the item on
    // the top edge, or otherwise the item directly to the left. This assumes that at at
    // least the item to the left has already been loaded. If the top edge item has been
    // loaded, we use that instead, as it allows us to load multiple inner row/column items in one go.

    if (fxViewItem->index == currentLayoutRequest.topLeftIndex) {
        // Special case: the top left item has pos 0, 0 for now
        return 0;
    }

    bool isTopEdgeItem = rowAtIndex(fxViewItem->index) == rowAtIndex(currentLayoutRequest.topLeftIndex);

    if (isTopEdgeItem) {
        FxViewItem *fxViewAnchorItem = itemLeftOf(fxViewItem);
        Q_ASSERT(fxViewAnchorItem);
        QQuickItem *anchorItem = fxViewAnchorItem->item;
        return anchorItem->position().x() + anchorItem->size().width() + columnSpacing;
    } else {
        FxViewItem *fxViewAnchorItem = itemOnTopEdgeOf(fxViewItem);
        Q_ASSERT(fxViewAnchorItem);
        QQuickItem *anchorItem = fxViewAnchorItem->item;
        return anchorItem->position().x();
    }
}

qreal QQuickTableViewPrivate::calculateTablePositionY(FxTableItemSG *fxViewItem)
{
    // Calculate the table position y of fxViewItem based on the position of the item on
    // the left edge, or otherwise the item directly on top. This assumes that at at
    // least the item on top has already been loaded. If the left edge item has been
    // loaded, we use that instead, as it allows us to load multiple inner row/column items in one go.

    if (fxViewItem->index == currentLayoutRequest.topLeftIndex) {
        // Special case: the top left item has pos 0, 0 for now
        return 0;
    }

    bool isLeftEdgeItem = columnAtIndex(fxViewItem->index) == columnAtIndex(currentLayoutRequest.topLeftIndex);

    if (isLeftEdgeItem) {
        FxViewItem *fxViewAnchorItem = itemOnTopOf(fxViewItem);
        Q_ASSERT(fxViewAnchorItem);
        QQuickItem *anchorItem = fxViewAnchorItem->item;
        return anchorItem->position().y() + anchorItem->size().height() + rowSpacing;
    } else {
        FxViewItem *fxViewAnchorItem = itemOnLeftEdgeOf(fxViewItem);
        Q_ASSERT(fxViewAnchorItem);
        QQuickItem *anchorItem = fxViewAnchorItem->item;
        return anchorItem->position().y();
    }
}

bool QQuickTableViewPrivate::canHaveMoreItemsBelow(FxTableItemSG *fxViewItem)
{
    int row = rowAtIndex(fxViewItem->index);
    if (row > rows)
        return false;

    QQuickItem *tableItemItem = fxViewItem->item;
    QRectF itemRect = QRectF(tableItemItem->position(), tableItemItem->size());
    return itemRect.bottomRight().y() < currentLayoutRequest.viewportRect.bottomRight().y();
}

bool QQuickTableViewPrivate::canHaveMoreItemsRightOf(FxTableItemSG *fxViewItem)
{
    int column = columnAtIndex(fxViewItem->index);
    if (column > columns)
        return false;

    QQuickItem *tableItemItem = fxViewItem->item;
    QRectF itemRect = QRectF(tableItemItem->position(), tableItemItem->size());
    return itemRect.bottomRight().x() < currentLayoutRequest.viewportRect.bottomRight().x();
}

void QQuickTableViewPrivate::bookkeepRequest(const QRectF viewportRect)
{
    if (viewportRect == currentLayoutRequest.viewportRect) {
        qCDebug(lcItemViewDelegateLifecycle) << "clearing pending content rect";
        currentLayoutRequest.pendingViewportRect = QRectF();
    } else {
        qCDebug(lcItemViewDelegateLifecycle) << "assigning pending content rect" << viewportRect;
        currentLayoutRequest.pendingViewportRect = viewportRect;
    }
}

void QQuickTableViewPrivate::requestNextLeftEdgeItem(FxTableItemSG *fxTableItem)
{
    if (canHaveMoreItemsBelow(fxTableItem)) {
        int row = rowAtIndex(fxTableItem->index);
        currentLayoutRequest.requestedLeftEdgeIndex = indexAt(row + 1, 0);
        requestTableItemAsync(currentLayoutRequest.requestedLeftEdgeIndex);
    } else {
        // Mark that we're done loading left edge items
        currentLayoutRequest.requestedLeftEdgeIndex = kNullValue;
    }
}

void QQuickTableViewPrivate::requestNextTopEdgeItem(FxTableItemSG *fxTableItem)
{
    if (canHaveMoreItemsRightOf(fxTableItem)) {
        int column = columnAtIndex(fxTableItem->index);
        currentLayoutRequest.requestedTopEdgeIndex = indexAt(0, column + 1);
        requestTableItemAsync(currentLayoutRequest.requestedTopEdgeIndex);
    } else {
        // Mark that we're done loading top edge items
        currentLayoutRequest.requestedTopEdgeIndex = kNullValue;
    }
}

void QQuickTableViewPrivate::calculateTableGridMetrics()
{
    // When all edge items of the table has been loaded, we have enough
    // information to calculate column widths and row height etc.
//    int rowCount = currentLayoutRequest.visualRowCount;
//    int columnCount = currentLayoutRequest.visualColumnCount;
//    int topLeftRow = rowAtIndex(currentLayoutRequest.topLeftIndex);
//    int topLeftColumn = columnAtIndex(currentLayoutRequest.topLeftIndex);

//    currentLayoutRequest.topRightIndex = indexAt(topLeftRow, topLeftColumn + columnCount - 1);
//    currentLayoutRequest.bottomLeftIndex = indexAt(topLeftRow + rowCount - 1, topLeftColumn);
//    currentLayoutRequest.bottomRightIndex = indexAt(topLeftRow + rowCount - 1, topLeftColumn + columnCount - 1);

//    qCDebug(lcItemViewDelegateLifecycle) << "row count" << rowCount << "column count:" << columnCount;
//    qCDebug(lcItemViewDelegateLifecycle) << "top left:" << indexToString(currentLayoutRequest.topLeftIndex);
//    qCDebug(lcItemViewDelegateLifecycle) << "bottom left:" << indexToString(currentLayoutRequest.bottomLeftIndex);
//    qCDebug(lcItemViewDelegateLifecycle) << "top right:" << indexToString(currentLayoutRequest.topRightIndex);
//    qCDebug(lcItemViewDelegateLifecycle) << "bottom right:" << indexToString(currentLayoutRequest.bottomRightIndex);
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
    if (loadRequests.isEmpty()) {
        qCDebug(lcItemViewDelegateLifecycle) << "no more pending load requests";
        currentLayoutRequest.state = GridLayoutRequest::Idle;
        return;
    }

    currentLayoutRequest.state = GridLayoutRequest::LoadingTableRequest;
    const TableSectionLoadRequest request = loadRequests.head();
    qCDebug(lcItemViewDelegateLifecycle) << request;

    if (request.loadMode == TableSectionLoadRequest::LoadOneByOne) {
        int index = indexAt(request.startRow, request.startColumn);
        requestTableItemAsync(index);
    } else {
        int topLeftRow = rowAtIndex(currentLayoutRequest.topLeftIndex);
        int topLeftColumn = columnAtIndex(currentLayoutRequest.topLeftIndex);
        int rows = currentLayoutRequest.bottomLeftRow - topLeftRow + 1;
        int columns = currentLayoutRequest.topRightColumn - topLeftColumn + 1;
        qCDebug(lcItemViewDelegateLifecycle) << "rows/columns:" << rows << columns;

        for (int y = request.startRow; y < rows; ++y) {
            for (int x = request.startColumn; x < columns; ++x) {
                currentLayoutRequest.requestedItemCount++;
                int index = indexAt(topLeftRow + y, topLeftColumn + x);
                requestTableItemAsync(index);
            }
        }

        if (request.fillDirection & Qt::RightEdge) {

        }
//        currentLayoutRequest.requestedItemCount = x;
//        switch (fillDirection) {
//        case Qt::RightEdge:
//            break;
//        default:
//            Q_UNREACHABLE();
//        }
    }
}

void QQuickTableViewPrivate::reloadTable(const QRectF &viewportRect)
{
    qCDebug(lcItemViewDelegateLifecycle());

    releaseVisibleItems();

    currentLayoutRequest = GridLayoutRequest(viewportRect);
    currentLayoutRequest.topLeftIndex = indexAt(0, 0);

    int topLeftRow = rowAtIndex(currentLayoutRequest.topLeftIndex);
    int topLeftColumn = columnAtIndex(currentLayoutRequest.topLeftIndex);

    TableSectionLoadRequest requestEdgeRow;
    requestEdgeRow.startRow = topLeftRow;
    requestEdgeRow.startColumn = topLeftColumn;
    requestEdgeRow.fillDirection = Qt::RightEdge;
    requestEdgeRow.loadMode = TableSectionLoadRequest::LoadOneByOne;
    enqueueLoadRequest(requestEdgeRow);

    TableSectionLoadRequest requestEdgeColumn;
    requestEdgeColumn.startRow = topLeftRow + 1;
    requestEdgeColumn.startColumn = topLeftColumn;
    requestEdgeColumn.fillDirection = Qt::BottomEdge;
    requestEdgeColumn.loadMode = TableSectionLoadRequest::LoadOneByOne;
    enqueueLoadRequest(requestEdgeColumn);

    TableSectionLoadRequest requestInnerItems;
    requestInnerItems.startRow = topLeftRow + 1;
    requestInnerItems.startColumn = topLeftColumn + 1;
    requestInnerItems.fillDirection = Qt::RightEdge | Qt::BottomEdge;
    requestInnerItems.loadMode = TableSectionLoadRequest::LoadInParallel;
    enqueueLoadRequest(requestInnerItems);

    executeNextLoadRequest();
}

void QQuickTableViewPrivate::addVisibleItems(const QRectF viewportRect)
{
    if (visibleItems.isEmpty()) {
        // Do a complete refill from scratch
        qCDebug(lcItemViewDelegateLifecycle) << "create table from scratch" << viewportRect;
        reloadTable(viewportRect);
    } else {
        // Refill items around the already visible table items

        currentLayoutRequest.viewportRect = viewportRect;

        FxTableItemSG *topRightItem = visibleTableItem(indexAt(rowAtIndex(currentLayoutRequest.topLeftIndex), currentLayoutRequest.topRightColumn));
        FxTableItemSG *bottomLeftItem = visibleTableItem(indexAt(currentLayoutRequest.bottomLeftRow, columnAtIndex(currentLayoutRequest.topLeftIndex)));

        if (canHaveMoreItemsRightOf(topRightItem)) {
            // todo: figure out how many columns I need to load
            // And, no need to load all items one by one
            TableSectionLoadRequest columnRequest;
            columnRequest.startRow = rowAtIndex(currentLayoutRequest.topLeftIndex);
            columnRequest.startColumn = currentLayoutRequest.topRightColumn + 1;
            columnRequest.fillDirection = Qt::BottomEdge;
            columnRequest.loadMode = TableSectionLoadRequest::LoadOneByOne;
            enqueueLoadRequest(columnRequest);
        }

        if (canHaveMoreItemsBelow(bottomLeftItem)) {
            TableSectionLoadRequest rowrequest;
            rowrequest.startRow = currentLayoutRequest.bottomLeftRow + 1;
            rowrequest.startColumn = columnAtIndex(currentLayoutRequest.topLeftIndex);
            rowrequest.fillDirection = Qt::RightEdge;
            rowrequest.loadMode = TableSectionLoadRequest::LoadOneByOne;
            enqueueLoadRequest(rowrequest);
        }

        if (!loadRequests.isEmpty())
            executeNextLoadRequest();
    }
}

bool QQuickTableViewPrivate::addRemoveVisibleItems()
{
    Q_Q(QQuickTableView);

    QRectF viewportRect = QRectF(QPointF(q->contentX(), q->contentY()), q->size());
    if (!viewportRect.isValid())
        return false;

    if (currentLayoutRequest.state != GridLayoutRequest::Idle) {
        // We are currently working on something else. Bookkeep the request and leave.
        bookkeepRequest(viewportRect);
        return false;
    }

    addVisibleItems(viewportRect);

    if (forceSynchronousMode)
        deliverPostedTableItems();

    // return false? or override caller?
    return true;
}

void QQuickTableViewPrivate::tableItemLoaded(int index)
{
    qCDebug(lcItemViewDelegateLifecycle) << indexToString(index);

    FxTableItemSG *fxTableItem = getTableItem(index);
    int x = calculateTablePositionX(fxTableItem);
    int y = calculateTablePositionY(fxTableItem);
    fxTableItem->setPosition(QPointF(x, y));
    showTableItem(fxTableItem);

    int row = rowAtIndex(index);
    int column = columnAtIndex(index);

    // Bookkeep the edge rows/columns we have encountered so far
    currentLayoutRequest.topRightColumn = qMax(column, currentLayoutRequest.topRightColumn);
    currentLayoutRequest.bottomLeftRow = qMax(row, currentLayoutRequest.bottomLeftRow);

    const TableSectionLoadRequest request = loadRequests.head();

    if (request.loadMode == TableSectionLoadRequest::LoadOneByOne) {
        // Continue loading the next adjacent item according to fill direction
        switch (request.fillDirection) {
        case Qt::RightEdge:
            if (canHaveMoreItemsRightOf(fxTableItem)) {
                int index = indexAt(row, column + 1);
                requestTableItemAsync(index);
            } else {
                dequeueCurrentLoadRequest();
                executeNextLoadRequest();
            }
            break;
        case Qt::BottomEdge:
            if (canHaveMoreItemsBelow(fxTableItem)) {
                int index = indexAt(row + 1, column);
                requestTableItemAsync(index);
            } else {
                dequeueCurrentLoadRequest();
                executeNextLoadRequest();
            }
            break;
        default:
            Q_UNREACHABLE();
        }
    } else {
        if (--currentLayoutRequest.requestedItemCount == 0) {
            dequeueCurrentLoadRequest();
            executeNextLoadRequest();
        }
    }
}

void QQuickTableViewPrivate::checkForPendingRequests()
{
    // While we were processing the current layoutRequest, the table view might
    // have been scrolled to a different place than currentLayoutRequest.viewportRect.
    // If so, currentLayoutRequest.pendingViewportRect will contain this
    // rect, and since we're now done with currentLayoutRequest, we continue with the pending one.
    qCDebug(lcItemViewDelegateLifecycle);
    if (currentLayoutRequest.pendingViewportRect.isValid()) {
        currentLayoutRequest = GridLayoutRequest(currentLayoutRequest.pendingViewportRect);
        qCDebug(lcItemViewDelegateLifecycle) << "should continue with request..." << currentLayoutRequest.viewportRect;
//        reloadTable();
    } else {
        currentLayoutRequest.state = GridLayoutRequest::Idle;
        qCDebug(lcItemViewDelegateLifecycle) << "done!";
    }
}

void QQuickTableViewPrivate::createTableItems(int fromColumn, int toColumn, int fromRow, int toRow)
{
    // Create and position all items in the table section described by the arguments
    qCDebug(lcItemViewDelegateLifecycle) << "create items ( x1:"
                                         << fromColumn << ", x2:" << toColumn << ", y1:"
                                         << fromRow << ", y2:" << toRow  << ")";

    for (int row = fromRow; row <= toRow; ++row) {
        qreal cachedRowHeight = getCachedRowHeight(row);

        for (int col = fromColumn; col <= toColumn; ++col) {
            qreal cachedColumnWidth = getCachedColumnWidth(col);

            FxTableItemSG *item = createTableItem(indexAt(row, col), QQmlIncubator::Synchronous);
            if (!item) {
                // TODO: This will happen if item get loaded async.
                if (cachedRowHeight == kNullValue || cachedColumnWidth == kNullValue) {
                    // We need this item before we can continue layouting, to decide row/col size
                }
                continue;
            }

            visibleItems.append(item);

            // Problem: cellPosition depends on column width for all columns up to the given column.
            // And currently, columnWidth() does not use columnWidthCache.
            // 1. check if the cell/current grid geometry cache is empty for (row, col), and if so, assign item width/height to it here directly.
            // 2. columnWidth() should first check the explicit column width hash, and if not found, check the grid
            //		cache, and if not found, create a dummy item to read the size, and put it in the cache.
            // 3. The cache size should be based on buffer size. And cleared whenever we release a row/column.

           // QRectF cellGeometry = getCellGeometry(item);

            if (cachedRowHeight == kNullValue) {
                // Since the height of the current row has not yet been cached, we cache
                // it now based on the first item we encountered for this row.
                cachedRowHeight = item->item->size().height();
                rowHeightCache_unused[row] = cachedRowHeight;
            }

            if (cachedColumnWidth == kNullValue) {
                // Since the width of the current column has not yet been cached, we cache
                // it now based on the first item we encountered for this column.
                cachedColumnWidth = item->item->size().width();
                columnWidthCache_unused[col] = cachedColumnWidth;
            }

            item->setPosition(cellPosition(row, col), true);
            item->setSize(QSizeF(cachedColumnWidth, cachedRowHeight), true);
        }
    }
}


void QQuickTableViewPrivate::fillTable()
{
    // XXX: why do we update itemCount here? What is it used for? It contains the
    // number of items in the model, which can be thousands...
    itemCount = model->count();

    QRectF r = currentLayoutRequest.viewportRect;

    // Get grid corners. This informations needs to be known before we can
    // do any useful layouting.
    int x1 = columnAtPos(r.x());
    int x2 = columnAtPos(r.right());
    int y1 = rowAtPos(r.y());
    int y2 = rowAtPos(r.bottom());
    QMargins currentGrid(x1, y1, x2, y2);

    bool itemsRemoved = removeItemsOutsideView(prevGrid, currentGrid);
    bool itemsAdded = addItemsInsideView(prevGrid, currentGrid);

    prevGrid = currentGrid;

    if (itemsRemoved || itemsAdded) {
        qCDebug(lcItemViewDelegateLifecycle) << "visible items count:" << visibleItems.count();
        //TODO: cannot return true, so issue repaint somehow
    }
    //TODO: cannot return true, so issue repaint somehow
}

bool QQuickTableViewPrivate::removeItemsOutsideView(const QMargins &previousGrid, const QMargins &currentGrid)
{
    bool itemsRemoved = false;

    int previousGridTopClamped = qBound(currentGrid.top(), previousGrid.top(), currentGrid.bottom());
    int previousGridBottomClamped = qBound(currentGrid.top(), previousGrid.bottom(), currentGrid.bottom());

    if (previousGrid.left() < currentGrid.left()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed out left";
        releaseItems(previousGrid.left(), currentGrid.left() - 1, previousGridTopClamped, previousGridBottomClamped);
        itemsRemoved = true;
    } else if (previousGrid.right() > currentGrid.right()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed out right";
        releaseItems(currentGrid.right() + 1, previousGrid.right(), previousGridTopClamped, previousGridBottomClamped);
        itemsRemoved = true;
    }

    if (previousGrid.top() < currentGrid.top()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed out on top";
        releaseItems(previousGrid.left(), previousGrid.right(), previousGrid.top(), currentGrid.top() - 1);
        itemsRemoved = true;
    } else if (previousGrid.bottom()> currentGrid.bottom()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed out at bottom";
        releaseItems(previousGrid.left(), previousGrid.right(), currentGrid.bottom() + 1, previousGrid.bottom());
        itemsRemoved = true;
    }

    return itemsRemoved;
}

bool QQuickTableViewPrivate::addItemsInsideView(const QMargins &previousGrid, const QMargins &currentGrid)
{
    bool itemsAdded = false;

    int previousGridTopClamped = qBound(currentGrid.top(), previousGrid.top(), currentGrid.bottom());
    int previousGridBottomClamped = qBound(currentGrid.top(), previousGrid.bottom(), currentGrid.bottom());

    if (currentGrid.left() < previousGrid.left()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed in left";
        createTableItems(currentGrid.left(), previousGrid.left() - 1, previousGridTopClamped, previousGridBottomClamped);
        itemsAdded = true;
    } else if (currentGrid.right() > previousGrid.right()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed in right";
        createTableItems(previousGrid.right() + 1, currentGrid.right(), previousGridTopClamped, previousGridBottomClamped);
        itemsAdded = true;
    }

    if (currentGrid.top() < previousGrid.top()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed in on top";
        createTableItems(currentGrid.left(), currentGrid.right(), currentGrid.top(), previousGrid.top() - 1);
        itemsAdded = true;
    } else if (currentGrid.bottom() > previousGrid.bottom()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed in at bottom";
        createTableItems(currentGrid.left(), currentGrid.right(), previousGrid.bottom() + 1, currentGrid.bottom());
        itemsAdded = true;
    }

    return itemsAdded;
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
    return qMax(0, d->rows);
}

void QQuickTableView::setRows(int rows)
{
    Q_D(QQuickTableView);
    if (d->rows == rows)
        return;

    d->rows = rows;
    if (d->componentComplete && d->model && d->ownModel)
        static_cast<QQmlDelegateModel *>(d->model.data())->setRows(rows);

    d->updateViewportContentHeight();

    emit rowsChanged();
}

void QQuickTableView::resetRows()
{
    Q_D(QQuickTableView);
    if (d->rows == -1)
        return;

    d->rows = -1;
    if (d->componentComplete && d->model && d->ownModel)
        static_cast<QQmlDelegateModel *>(d->model.data())->resetRows();
    emit rowsChanged();
}

int QQuickTableView::columns() const
{
    return d_func()->columns;
}

void QQuickTableView::setColumns(int columns)
{
    Q_D(QQuickTableView);
    if (d->columns == columns)
        return;

    d->columns = columns;
    if (d->componentComplete && d->model && d->ownModel)
        static_cast<QQmlDelegateModel *>(d->model.data())->setColumns(columns);

    d->updateViewportContentWidth();

    emit columnsChanged();
}

void QQuickTableView::resetColumns()
{
    Q_D(QQuickTableView);
    if (d->columns == -1)
        return;

    d->columns = -1;
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

qreal QQuickTableView::columnWidth(int column)
{
    return d_func()->columnWidth(column);
}

qreal QQuickTableView::rowHeight(int row)
{
    return d_func()->rowHeight(row);
}

bool QQuickTableView::event(QEvent *e)
{
    Q_D(QQuickTableView);

    if (e->type() == d->eventTypeDeliverTableItems) {
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
        if (d->rows > 0)
            delegateModel->setRows(d->rows);
        if (d->columns > 0)
            delegateModel->setColumns(d->columns);
        static_cast<QQmlDelegateModel *>(d->model.data())->componentComplete();
    }

    // NB: deliberatly skipping QQuickAbstractItemView, since it does so
    // many wierd that I don't need or understand. That code is messy...
    QQuickFlickable::componentComplete();
}

QT_END_NAMESPACE
