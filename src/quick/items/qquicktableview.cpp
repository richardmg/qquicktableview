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

QT_BEGIN_NAMESPACE

static const int kNullValue = -1;

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

class GridLayoutRequest
{
public:
    enum LayoutState {
        NoState = 0,
        ReadyToStart,
        WaitingForAnchorItem,
        LoadingKeyItems,
        LoadingInnerItems,
        Done
    };

    GridLayoutRequest(const QRectF &visibleContentRect)
        : state(ReadyToStart)
        , visibleContentRect(visibleContentRect)
        , waitingForIndex(kNullValue)
        , loadedWidth(0)
        , loadedHeight(0)
    {}

    LayoutState state;
    QRectF visibleContentRect;
    int waitingForIndex;
    qreal loadedWidth;
    qreal loadedHeight;

    bool isProcessing()
    {
        return state > ReadyToStart && state < Done;
    }
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
    bool createdItemSyncBlocker;

    QMargins prevGrid;
    mutable QPair<int, qreal> rowPositionCache;
    mutable QPair<int, qreal> columnPositionCache;
    QVector<qreal> rowHeightCache;
    QVector<qreal> columnWidthCache;

    bool inViewportMoved;

    void updateViewportContentWidth();
    void updateViewportContentHeight();

    FxTableItemSG *createTableItem(int index, QQmlIncubator::IncubationMode mode);
    void createTableItems(int fromColumn, int toColumn, int fromRow, int toRow);
    void releaseItems(int fromColumn, int toColumn, int fromRow, int toRow);
    bool removeItemsOutsideView(const QMargins &previousGrid, const QMargins &currentGrid);
    bool addItemsInsideView(const QMargins &previousGrid, const QMargins &currentGrid);
    qreal getCachedRowHeight(int row);
    qreal getCachedColumnWidth(int col);
    void fillTable();

    void continueCurrentLayoutRequest();
    void continueReadyToStart();
    void continueWaitingForAnchorItem();
    void continueDone();

    QString indexToString(int index);
};

QQuickTableViewPrivate::QQuickTableViewPrivate()
    : rows(-1),
      columns(-1),
      visibleRows(0),
      visibleColumns(0),
      orientation(QQuickTableView::Vertical),
      rowSpacing(0),
      columnSpacing(0),
      prevContentPos(QPointF(0, 0)),
      prevViewSize(QSizeF(0, 0)),
      currentLayoutRequest(QRect()),
      createdItemSyncBlocker(false),
      prevGrid(QMargins(-1, -1, -1, -1)),
      rowPositionCache(qMakePair(0, 0)),
      columnPositionCache(qMakePair(0, 0)),
      rowHeightCache(QVector<qreal>(100, kNullValue)),
      columnWidthCache(QVector<qreal>(100, kNullValue)),
      inViewportMoved(false)
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
    if (index < 0 || index >= itemCount || columns <= 0)
        return -1;
    return index / columns;
}

int QQuickTableViewPrivate::columnAtIndex(int index) const
{
    Q_Q(const QQuickTableView);
    int columns = q->columns();
    if (index < 0 || index >= itemCount || columns <= 0)
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
    int rowHeight = rowHeightCache[row];
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
    int columnWidth = columnWidthCache[column];
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

    d->refillOrLayout();

    d->inViewportMoved = false;
}

void QQuickTableView::createdItem(int index, QObject* object)
{
    Q_D(QQuickTableView);

    qCDebug(lcItemViewDelegateLifecycle) << index;

    if (index == d->requestedIndex) {
        // This is needed by the model. Essientially the same as GridLayoutRequest::waitingForIndex.
        // TODO: let model have it's own handler, and refactor requestedIndex out the view?
        d->requestedIndex = -1;
    }

    if (d->createdItemSyncBlocker) {
        // The item was created synchronously. To avoid recursions on the stack, we ignore
        // the callback, and continue from the place where the item was requested.
        return;
    }

    Q_ASSERT(qmlobject_cast<QQuickItem*>(object));

    // Currently we only ask for one item at a time, the one needed to continue with the
    // layout. But an optimization here is to ask for more items in parallel, and instead
    // of asserting, just return until we get the item we waiting for.
    Q_ASSERT(d->currentLayoutRequest.waitingForIndex == index);

    d->continueCurrentLayoutRequest();
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

    // Show the item
    QQuickItemPrivate::get(item->item)->setCulled(false);

    return item;
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
                rowHeightCache[row] = cachedRowHeight;
            }

            if (cachedColumnWidth == kNullValue) {
                // Since the width of the current column has not yet been cached, we cache
                // it now based on the first item we encountered for this column.
                cachedColumnWidth = item->item->size().width();
                columnWidthCache[col] = cachedColumnWidth;
            }

            item->setPosition(cellPosition(row, col), true);
            item->setSize(QSizeF(cachedColumnWidth, cachedRowHeight), true);
        }
    }
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

bool QQuickTableViewPrivate::addRemoveVisibleItems()
{
    Q_Q(QQuickTableView);

    if (currentLayoutRequest.isProcessing()) {
        // We are already processesing a layout request, so wait for it to finish.
        // todo: should I post a new polish here, to handle the new request later?
        return false;
    }

    QRectF visibleContentRect = QRectF(QPointF(q->contentX(), q->contentY()), q->size());
    qCDebug(lcItemViewDelegateLifecycle) << "Creating new layout request:" << visibleContentRect;

    currentLayoutRequest = GridLayoutRequest(visibleContentRect);
    continueCurrentLayoutRequest();
    return true;
}

void QQuickTableViewPrivate::continueCurrentLayoutRequest()
{
    // Items will be created by the model as we continue with the layout request. The
    // model will first check if items are already in the cache, and if not, create them
    // async or sync depending on the incubation flags. For items that are created async, we'll
    // get callbacks once they're done incubated, which will lead to this function being called
    // again. We can then continue where we left off. But since we also get callbacks for items
    // that are incubated sync, we need a blocker to avoid recursion.
    if (createdItemSyncBlocker)
        return;
    QBoolBlocker ignoreCreatedItemCallback(createdItemSyncBlocker);

    qCDebug(lcItemViewDelegateLifecycle) << "Enter";

    GridLayoutRequest::LayoutState layoutState = GridLayoutRequest::NoState;

    while (layoutState != currentLayoutRequest.state) {
        layoutState = currentLayoutRequest.state;

        switch (layoutState) {
        case GridLayoutRequest::ReadyToStart:
            continueReadyToStart();
            break;
        case GridLayoutRequest::WaitingForAnchorItem:
            continueWaitingForAnchorItem();
            break;
        case GridLayoutRequest::Done:
            continueDone();
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }

    qCDebug(lcItemViewDelegateLifecycle) << "Leave";
}

QString QQuickTableViewPrivate::indexToString(int index)
{
    return QString::fromLatin1("index: %1 (%2, %3)").arg(index).arg(rowAtIndex(index)).arg(columnAtIndex(index));
}

void QQuickTableViewPrivate::continueReadyToStart()
{
    // The first state is all about figuring out which cell to start loading.
    // This will be an anchor that we can layout the rest of the items from.
    // The actual loading will be done from continueWaitingForAnchorItem().
    currentLayoutRequest.state = GridLayoutRequest::WaitingForAnchorItem;
    currentLayoutRequest.waitingForIndex = indexAt(0, 0);
    qCDebug(lcItemViewDelegateLifecycle) << "Anchor item:" << indexToString(currentLayoutRequest.waitingForIndex);
}

void QQuickTableViewPrivate::continueWaitingForAnchorItem()
{
    FxTableItemSG *tableItem = createTableItem(currentLayoutRequest.waitingForIndex, QQmlIncubator::Asynchronous);
    qCDebug(lcItemViewDelegateLifecycle) << "Anchor item" << indexToString(currentLayoutRequest.waitingForIndex) << "loaded?" << bool(tableItem);
    if (!tableItem)
        return;

    visibleItems.append(tableItem);
    currentLayoutRequest.state = GridLayoutRequest::Done;
}

void QQuickTableViewPrivate::continueDone()
{
    qCDebug(lcItemViewDelegateLifecycle) << "Request done";
}

void QQuickTableViewPrivate::fillTable()
{
    // XXX: why do we update itemCount here? What is it used for? It contains the
    // number of items in the model, which can be thousands...
    itemCount = model->count();

    QRectF r = currentLayoutRequest.visibleContentRect;

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
    }
    QQuickAbstractItemView::componentComplete();
}

QT_END_NAMESPACE
