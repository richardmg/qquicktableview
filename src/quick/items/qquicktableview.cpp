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

    QPointF itemPosition(int row, int column) const;
    QPointF itemPosition(FxViewItem *item) const;
    QPointF itemEndPosition(FxViewItem *item) const;
    QSizeF itemSize(int row, int column) const;
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

    void debug_removeAllItems();

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
    QRectF prevContentRect;
    mutable QPair<int, qreal> rowPositionCache;
    mutable QPair<int, qreal> columnPositionCache;

    bool inViewportMoved;

    void updateViewportContentWidth();
    void updateViewportContentHeight();

    void createAndPositionItem(int row, int col);
    void createAndPositionItems(int fromColumn, int toColumn, int fromRow, int toRow);
    void releaseItems(int fromColumn, int toColumn, int fromRow, int toRow);
    bool addRemoveVisibleItemsOnEdges(const QMargins &previousGrid, const QMargins &currentGrid);
    bool addRemoveAllVisibleItems(const QMargins &currentGrid, bool overlapHint);
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
      prevContentRect(QRectF(0, 0, 0, 0)),
      rowPositionCache(qMakePair(0, 0)),
      columnPositionCache(qMakePair(0, 0)),
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
    for (FxViewItem *item : visibleItems) {
        int index = item->index;
        if (rowAtIndex(index) == row && columnAtIndex(index) == column)
            return item;
    }

    return nullptr;
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
    srand(row);
    int r = ((rand() % 40) * 2) + 20;
    return r;

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
    srand(column);
    int r = ((rand() % 100) * 2) + 50;
    return r;

//    int row = rowAt(visibleIndex);
//    FxViewItem *item = visibleItemAt(row, column);
//    return item ? item->itemWidth() : averageSize.width();
}

void QQuickTableViewPrivate::updateViewportContentWidth()
{
    Q_Q(QQuickTableView);
    qreal contentWidth = 0;
    for (int i = 0; i < columns; ++i)
        contentWidth += columnWidth(i);
    contentWidth += (columns - 1) * columnSpacing;
    q->setContentWidth(contentWidth);
}

void QQuickTableViewPrivate::updateViewportContentHeight()
{
    Q_Q(QQuickTableView);
    qreal contentHeight = 0;
    for (int i = 0; i < rows; ++i)
        contentHeight += rowHeight(i);
    contentHeight += (rows - 1) * columnSpacing;
    q->setContentHeight(contentHeight);
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

QPointF QQuickTableViewPrivate::itemPosition(int row, int column) const
{
   return QPointF(columnPos(column), rowPos(row));
}

QPointF QQuickTableViewPrivate::itemPosition(FxViewItem *item) const
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

QSizeF QQuickTableViewPrivate::itemSize(int row, int column) const
{
    return QSizeF(columnWidth(column), rowHeight(row));
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

bool QQuickTableViewPrivate::addRemoveVisibleItems()
{
    Q_Q(QQuickTableView);

    bufferPause.stop(); // ###
    currentChanges.reset(); // ###

    QPointF from, to;
    QSizeF tableViewSize = size();
    tableViewSize.setWidth(qMax<qreal>(tableViewSize.width(), 0.0));
    tableViewSize.setHeight(qMax<qreal>(tableViewSize.height(), 0.0));

    QPointF contentPos = position();

    if (isContentFlowReversed()) {
        from = -contentPos /*- displayMarginBeginning*/ - tableViewSize;
        to = -contentPos /*+ displayMarginEnd*/;
    } else {
        from = contentPos /*- displayMarginBeginning*/;
        to = contentPos /*+ displayMarginEnd*/ + tableViewSize;
    }

    // XXX: why do we update itemCount here? What is it used for? It contains the
    // number of items in the model, which can be thousands...
    itemCount = model->count();

    QRectF contentRect(QPointF(q->contentX(), q->contentY()), q->size());

    QPointF bufferFrom = from - QPointF(buffer, buffer);
    QPointF bufferTo = to + QPointF(buffer, buffer);
    QPointF fillFrom = from;
    QPointF fillTo = to;

    QMargins previousGrid(columnAtPos(prevContentRect.x()), rowAtPos(prevContentRect.y()),
                          columnAtPos(prevContentRect.right()), rowAtPos(prevContentRect.bottom()));
    QMargins currentGrid(columnAtPos(contentRect.x()), rowAtPos(contentRect.y()),
                         columnAtPos(contentRect.right()), rowAtPos(contentRect.bottom()));

    bool visibleItemsModified = false;
    const bool layoutChanged = contentRect.size() != prevContentRect.size() && currentGrid != previousGrid;
    const bool overlap = contentRect.intersects(prevContentRect);

    if (layoutChanged || !overlap || visibleItems.empty())
        visibleItemsModified = addRemoveAllVisibleItems(currentGrid, overlap);
    else
        visibleItemsModified = addRemoveVisibleItemsOnEdges(previousGrid, currentGrid);

    prevContentRect = contentRect;

    qCDebug(lcItemViewDelegateLifecycle) << "done adding/removing items. Visible items in table:" << visibleItems.count();
    return visibleItemsModified;
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

void QQuickTableViewPrivate::createAndPositionItem(int row, int col)
{
    int modelIndex = indexAt(row, col);
    FxTableItemSG *item = static_cast<FxTableItemSG *>(createItem(modelIndex, false));
    if (!item) {
        qCDebug(lcItemViewDelegateLifecycle) << "failed to create item for row/col:" << row << col;
        return;
    }

    if (!transitioner || !transitioner->canTransition(QQuickItemViewTransitioner::PopulateTransition, true)) {
        item->setPosition(itemPosition(row, col), true);
        item->setSize(itemSize(row, col), true);
    }

    if (item->item)
        QQuickItemPrivate::get(item->item)->setCulled(false);

    visibleItems.append(item);
}

void QQuickTableViewPrivate::createAndPositionItems(int fromColumn, int toColumn, int fromRow, int toRow)
{
    // Create and position all items in the table section described by the arguments
    qCDebug(lcItemViewDelegateLifecycle) << "create items ( x1:"
                                         << fromColumn << ", x2:" << toColumn << ", y1:"
                                         << fromRow << ", y2:" << toRow  << ")";

    for (int row = fromRow; row <= toRow; ++row) {
        for (int col = fromColumn; col <= toColumn; ++col)
            createAndPositionItem(row, col);
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

void QQuickTableViewPrivate::debug_removeAllItems()
{
    for (FxViewItem *item : visibleItems)
        releaseItem(item);

    releaseItem(currentItem);
    currentItem = 0;
    visibleItems.clear();
}

bool QQuickTableViewPrivate::addRemoveAllVisibleItems(const QMargins &currentGrid, bool overlapHint)
{
    qCDebug(lcItemViewDelegateLifecycle) << currentGrid << overlapHint;
    if (!overlapHint) {
        releaseVisibleItems();
        createAndPositionItems(currentGrid.left(), currentGrid.right(), currentGrid.top(), currentGrid.bottom());
    } else {
        // Since we expect the new grid to overlap with whatever was the grid before, we
        // take care not to unnecessary destroy all the QQuickItems in visibleItems. We do this by
        // creating the new FXViewItems first, before destroying the old ones, thereby utilizing the
        // ref-count system in QQmlInstanceModel to keep the ones that overlap alive.
        const QList<FxViewItem *> oldVisible = visibleItems;
        visibleItems.clear();
        createAndPositionItems(currentGrid.left(), currentGrid.right(), currentGrid.top(), currentGrid.bottom());
        for (FxViewItem *item : oldVisible)
            releaseItem(item);
    }

    return true;
}

bool QQuickTableViewPrivate::addRemoveVisibleItemsOnEdges(const QMargins &previousGrid, const QMargins &currentGrid)
{
    // Only add/remove the items on the edges that went into/out of the view
    bool visibleItemsModified = false;

    int previousGridTopClamped = qBound(currentGrid.top(), previousGrid.top(), currentGrid.bottom());
    int previousGridBottomClamped = qBound(currentGrid.top(), previousGrid.bottom(), currentGrid.bottom());

    if (previousGrid.left() < currentGrid.left()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed out left";
        releaseItems(previousGrid.left(), currentGrid.left() - 1, previousGridTopClamped, previousGridBottomClamped);
        visibleItemsModified = true;
    } else if (previousGrid.right() > currentGrid.right()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed out right";
        releaseItems(currentGrid.right() + 1, previousGrid.right(), previousGridTopClamped, previousGridBottomClamped);
        visibleItemsModified = true;
    }

    if (currentGrid.left() < previousGrid.left()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed in left";
        createAndPositionItems(currentGrid.left(), previousGrid.left() - 1, previousGridTopClamped, previousGridBottomClamped);
        visibleItemsModified = true;
    } else if (currentGrid.right() > previousGrid.right()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed in right";
        createAndPositionItems(previousGrid.right() + 1, currentGrid.right(), previousGridTopClamped, previousGridBottomClamped);
        visibleItemsModified = true;
    }

    if (previousGrid.top() < currentGrid.top()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed out on top";
        releaseItems(previousGrid.left(), previousGrid.right(), previousGrid.top(), currentGrid.top() - 1);
        visibleItemsModified = true;
    } else if (previousGrid.bottom()> currentGrid.bottom()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed out at bottom";
        releaseItems(previousGrid.left(), previousGrid.right(), currentGrid.bottom() + 1, previousGrid.bottom());
        visibleItemsModified = true;
    }

    if (currentGrid.top() < previousGrid.top()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed in on top";
        createAndPositionItems(currentGrid.left(), currentGrid.right(), currentGrid.top(), previousGrid.top() - 1);
        visibleItemsModified = true;
    } else if (currentGrid.bottom() > previousGrid.bottom()) {
        qCDebug(lcItemViewDelegateLifecycle) << "items pushed in at bottom";
        createAndPositionItems(currentGrid.left(), currentGrid.right(), previousGrid.bottom() + 1, currentGrid.bottom());
        visibleItemsModified = true;
    }

    return visibleItemsModified;
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
