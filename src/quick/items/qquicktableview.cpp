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

    void updateViewport() override;
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
    mutable QPair<int, qreal> columnPositionCache;

    bool inViewportMoved;

    void updateViewportContentWidth();
    void updateViewportContentHeight();

    bool addVisibleItems(const QPointF &fillFrom, const QPointF &fillTo,
                         const QPointF &bufferFrom, const QPointF &bufferTo, bool doBuffer);
    bool removeNonVisibleItems(const QPointF &fillFrom, const QPointF &fillTo,
                               const QPointF &bufferFrom, const QPointF &bufferTo);
    void createAndPositionItem(int row, int col, bool doBuffer);
    void createAndPositionItems(int fromColumn, int toColumn, int fromRow, int toRow, bool doBuffer);
    void releaseItems(int fromColumn, int toColumn, int fromRow, int toRow);
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

int QQuickTableViewPrivate::rowAtPos(qreal y) const
{
    // ### TODO: for now we assume all rows has the same height. This will not be the case in the end.
    // The strategy should either be to keep all the row heights in a separate array, or
    // inspect the current visible items to make a guess.
    return y / (rowHeight(0) + rowSpacing);
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

qreal QQuickTableViewPrivate::rowPos(int row) const
{
    // ### TODO: Support rows having different heights. Should this be stored in a separate list as well?
    return row * (rowHeight(row) + rowSpacing);

//    // ### TODO: bottom-to-top
//    int visibleColumn = columnAt(visibleIndex);
//    FxViewItem *item = visibleItemAt(row, visibleColumn);
//    if (item)
//        return item->itemY();

//    // estimate
//    int visibleRow = rowAt(visibleIndex);
//    if (row < visibleRow) {
//        int count = visibleRow - row;
//        FxViewItem *topLeft = visibleItemAt(visibleRow, visibleColumn);
//        return topLeft->itemY() - count * averageSize.height(); // ### spacing
//    } else if (row > visibleRow + visibleRows) {
//        int count = row - visibleRow - visibleRows;
//        FxViewItem *bottomLeft = visibleItemAt(visibleRow + visibleRows, visibleColumn);
//        return bottomLeft->itemY() + bottomLeft->itemHeight() + count * averageSize.height(); // ### spacing
//    }
//    return 0;
}

qreal QQuickTableViewPrivate::rowHeight(int row) const
{
    // ### TODO: Rather than search through visible items, I think this information should be
    // specified more explixit, e.g in an separate list. Use a constant for now.
    Q_UNUSED(row);
    return 60;

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
    switch(column) {
    case 0:
    case 6:
    case 8:
    case 20:
        return 200;
    }
    return 120;

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

void QQuickTableViewPrivate::updateViewport()
{
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

//    if (yflick()) {
//        if (d->isBottomToTop())
//            d->bufferMode = d->vData.smoothVelocity < 0 ? QQuickListViewPrivate::BufferAfter : QQuickListViewPrivate::BufferBefore;
//        else
//            d->bufferMode = d->vData.smoothVelocity < 0 ? QQuickListViewPrivate::BufferBefore : QQuickListViewPrivate::BufferAfter;
//    } else {
//        if (d->isRightToLeft())
//            d->bufferMode = d->hData.smoothVelocity < 0 ? QQuickListViewPrivate::BufferAfter : QQuickListViewPrivate::BufferBefore;
//        else
//            d->bufferMode = d->hData.smoothVelocity < 0 ? QQuickListViewPrivate::BufferBefore : QQuickListViewPrivate::BufferAfter;
//    }

    d->refillOrLayout();

    // Set visibility of items to eliminate cost of items outside the visible area.
//    qreal from = d->isContentFlowReversed() ? -d->position()-d->displayMarginBeginning-d->size() : d->position()-d->displayMarginBeginning;
//    qreal to = d->isContentFlowReversed() ? -d->position()+d->displayMarginEnd : d->position()+d->size()+d->displayMarginEnd;
//    for (FxViewItem *item : qAsConst(d->visibleItems)) {
//        if (item->item)
//            QQuickItemPrivate::get(item->item)->setCulled(d->itemEndPosition(item) < from || d->itemPosition(item) > to);
//    }
//    if (d->currentItem)
//        QQuickItemPrivate::get(d->currentItem->item)->setCulled(d->itemEndPosition(d->currentItem) < from || d->itemPosition(d->currentItem) > to);

//    if (d->hData.flicking || d->vData.flicking || d->hData.moving || d->vData.moving)
//        d->moveReason = QQuickListViewPrivate::Mouse;
//    if (d->moveReason != QQuickListViewPrivate::SetIndex) {
//        if (d->haveHighlightRange && d->highlightRange == StrictlyEnforceRange && d->highlight) {
//            // reposition highlight
//            qreal pos = d->itemPosition(d->highlight);
//            qreal viewPos = d->isContentFlowReversed() ? -d->position()-d->size() : d->position();
//            if (pos > viewPos + d->highlightRangeEnd - d->itemSize(d->highlight))
//                pos = viewPos + d->highlightRangeEnd - d->itemSize(d->highlight);
//            if (pos < viewPos + d->highlightRangeStart)
//                pos = viewPos + d->highlightRangeStart;
//            if (pos != d->itemPosition(d->highlight)) {
//                d->highlightPosAnimator->stop();
//                static_cast<FxListItemSG*>(d->highlight)->setPosition(pos);
//            } else {
//                d->updateHighlight();
//            }

//            // update current index
//            if (FxViewItem *snapItem = d->snapItemAt(d->itemPosition(d->highlight))) {
//                if (snapItem->index >= 0 && snapItem->index != d->currentIndex)
//                    d->updateCurrent(snapItem->index);
//            }
//        }
//    }

//    if ((d->hData.flicking || d->vData.flicking) && d->correctFlick && !d->inFlickCorrection) {
//        d->inFlickCorrection = true;
//        // Near an end and it seems that the extent has changed?
//        // Recalculate the flick so that we don't end up in an odd position.
//        if (yflick() && !d->vData.inOvershoot) {
//            if (d->vData.velocity > 0) {
//                const qreal minY = minYExtent();
//                if ((minY - d->vData.move.value() < height()/2 || d->vData.flickTarget - d->vData.move.value() < height()/2)
//                    && minY != d->vData.flickTarget)
//                    d->flickY(-d->vData.smoothVelocity.value());
//            } else if (d->vData.velocity < 0) {
//                const qreal maxY = maxYExtent();
//                if ((d->vData.move.value() - maxY < height()/2 || d->vData.move.value() - d->vData.flickTarget < height()/2)
//                    && maxY != d->vData.flickTarget)
//                    d->flickY(-d->vData.smoothVelocity.value());
//            }
//        }

//        if (xflick() && !d->hData.inOvershoot) {
//            if (d->hData.velocity > 0) {
//                const qreal minX = minXExtent();
//                if ((minX - d->hData.move.value() < width()/2 || d->hData.flickTarget - d->hData.move.value() < width()/2)
//                    && minX != d->hData.flickTarget)
//                    d->flickX(-d->hData.smoothVelocity.value());
//            } else if (d->hData.velocity < 0) {
//                const qreal maxX = maxXExtent();
//                if ((d->hData.move.value() - maxX < width()/2 || d->hData.move.value() - d->hData.flickTarget < width()/2)
//                    && maxX != d->hData.flickTarget)
//                    d->flickX(-d->hData.smoothVelocity.value());
//            }
//        }
//        d->inFlickCorrection = false;
//    }
//    if (d->hasStickyHeader())
//        d->updateHeader();
//    if (d->hasStickyFooter())
//        d->updateFooter();
//    if (d->sectionCriteria) {
//        d->updateCurrentSection();
//        d->updateStickySections();
//    }
    d->inViewportMoved = false;
}

bool QQuickTableViewPrivate::addRemoveVisibleItems()
{
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

    // XXX: what does buffer hold? Is it the same as cacheBuffer?
    QPointF bufferFrom = from - QPointF(buffer, buffer);
    QPointF bufferTo = to + QPointF(buffer, buffer);
    QPointF fillFrom = from;
    QPointF fillTo = to;

    bool added = addVisibleItems(fillFrom, fillTo, bufferFrom, bufferTo, false);
    bool removed = removeNonVisibleItems(fillFrom, fillTo, bufferFrom, bufferTo);

//    if (requestedIndex == -1 && buffer && bufferMode != NoBuffer) {
//        if (added) {
//            // We've already created a new delegate this frame.
//            // Just schedule a buffer refill.
//            bufferPause.start();
//        } else {
//            if (bufferMode & BufferAfter)
//                fillTo = bufferTo;
//            if (bufferMode & BufferBefore)
//                fillFrom = bufferFrom;
//            added |= addVisibleItems(fillFrom, fillTo, bufferFrom, bufferTo, true);
//        }
//    }
    return added || removed;
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

void QQuickTableViewPrivate::createAndPositionItem(int row, int col, bool doBuffer)
{
    int modelIndex = indexAt(row, col);
    FxTableItemSG *item = static_cast<FxTableItemSG *>(createItem(modelIndex, doBuffer));
    if (!item) {
        qCDebug(lcItemViewDelegateLifecycle) << "failed to create item for row/col:" << row << col;
        return;
    }

    if (!transitioner || !transitioner->canTransition(QQuickItemViewTransitioner::PopulateTransition, true)) {
        item->setPosition(itemPosition(row, col), true);
        item->setSize(itemSize(row, col), true);
    }

    if (item->item)
        QQuickItemPrivate::get(item->item)->setCulled(doBuffer);

    visibleItems.append(item);

    qCDebug(lcItemViewDelegateLifecycle) << "index:" << modelIndex
                                         << "row:" << row
                                         << "col:" << col
                                         << "pos:" << item->item->position()
                                         << "buffer:" << doBuffer
                                         << "item:" << (QObject *)(item->item)
                                            ;
}

void QQuickTableViewPrivate::createAndPositionItems(int fromColumn, int toColumn, int fromRow, int toRow, bool doBuffer)
{
    // Create and position all items in the table section described by the arguments
    for (int row = fromRow; row <= toRow; ++row) {
        for (int col = fromColumn; col <= toColumn; ++col)
            createAndPositionItem(row, col, doBuffer);
    }
}

void QQuickTableViewPrivate::releaseItems(int fromColumn, int toColumn, int fromRow, int toRow)
{
    for (int row = fromRow; row <= toRow; ++row) {
        for (int col = fromColumn; col <= toColumn; ++col) {
            FxViewItem *item = visibleItemAt(row, col);

            qCDebug(lcItemViewDelegateLifecycle) << "row:" << row
                                                 << "col:" << col
                                                 << "item:" << item
                                                    ;

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

bool QQuickTableViewPrivate::addVisibleItems(const QPointF &fillFrom, const QPointF &fillTo,
                                             const QPointF &bufferFrom, const QPointF &bufferTo, bool doBuffer)
{
    Q_Q(QQuickTableView);

    // Note: I'm currently not basing any calcualations on the arguments, only on the geometry of the
    // current content view. And I'm not sure why I would ever need to add items based on anything else.
    // This needs more investigtion!
    Q_UNUSED(fillFrom);
    Q_UNUSED(fillTo);
    Q_UNUSED(bufferFrom);
    Q_UNUSED(bufferTo);

    // For simplicity, we assume that we always specify the number of rows and columns directly
    // in TableView. But we should also allow those properties to be unspecified, and if so, get
    // the counts from the model instead. In addition, the counts might change inbetween two calls
    // to this function, which might cause previousBottomRow and previousRightColumn to end up wrong!
    // So we should probably store the last values used to be certain.
    int rowCount = rows;

    QPointF contentPos(q->contentX(), q->contentY());

    int previousTopRow = qMin(rowAtPos(prevContentPos.y()), rowCount - 1);
    int currentTopRow = rowAtPos(contentPos.y());
    int previousBottomRow = qMin(rowAtPos(prevContentPos.y() + height), rowCount - 1);
    int currentBottomRow = qMin(rowAtPos(contentPos.y() + height), rowCount - 1);

    int previousLeftColumn = columnAtPos(prevContentPos.x());
    int currentLeftColumn = columnAtPos(contentPos.x());
    int previousRightColumn = columnAtPos(prevContentPos.x() + width);
    int currentRightColumn = columnAtPos(contentPos.x() + width);

    // Check if the old rows and columns overlap with any of the new rows and columns we are about to create
    bool overlapsAbove = currentTopRow <= previousBottomRow && previousBottomRow <= currentBottomRow;
    bool overlapsBelow = currentTopRow <= previousTopRow && previousTopRow <= currentBottomRow;
    bool overlapsLeft = currentLeftColumn <= previousRightColumn && previousRightColumn <= currentRightColumn;
    bool overlapsRight = currentLeftColumn <= previousLeftColumn && previousLeftColumn <= currentRightColumn;

    qCDebug(lcItemViewDelegateLifecycle) << "\n\tfrom:" << fillFrom
                                         << "to:" << fillTo
                                         << "\n\tbufferFrom:" << bufferFrom
                                         << "bufferTo:" << bufferFrom
                                         << "\n\tcurrentTopRow:" << currentTopRow
                                         << "currentBottomRow:" << currentBottomRow
                                         << "\n\tcurrentLeftColumn:" << currentLeftColumn
                                         << "currentRightColumn:" << currentRightColumn
                                         << "\n\tpreviousLeftColumn:" << previousLeftColumn
                                         << "previousRightColumn:" << previousRightColumn
                                         << "\n\tpreviousTopRow:" << previousTopRow
                                         << "previousBottomRow:" << previousBottomRow
                                            ;

    if (visibleItems.isEmpty()) {
        qCDebug(lcItemViewDelegateLifecycle) << "create all visible items";
        createAndPositionItems(currentLeftColumn, currentRightColumn, currentTopRow, currentBottomRow, doBuffer);
    } else if (!((overlapsLeft || overlapsRight) && (overlapsAbove || overlapsBelow))) {
        qCDebug(lcItemViewDelegateLifecycle) << "table flicked more than a page, recreate all visible items";
        releaseItems(previousLeftColumn, previousRightColumn, previousTopRow, previousBottomRow);
        createAndPositionItems(currentLeftColumn, currentRightColumn, currentTopRow, currentBottomRow, doBuffer);
    } else {
        int previousTopRowClamped = qBound(currentTopRow, previousTopRow, currentBottomRow);
        int previousBottomRowClamped = qBound(currentTopRow, previousBottomRow, currentBottomRow);

        if (overlapsLeft) {
            qCDebug(lcItemViewDelegateLifecycle) << "releasing items outside view on the left, and padding out with new items on the right";
            releaseItems(previousLeftColumn, currentLeftColumn - 1, previousTopRowClamped, previousBottomRowClamped);
            createAndPositionItems(previousRightColumn + 1, currentRightColumn, previousTopRowClamped, previousBottomRowClamped, doBuffer);
        }

        if (overlapsRight) {
            qCDebug(lcItemViewDelegateLifecycle) << "releasing items outside view on the right, and padding out with new items on the left";
            releaseItems(currentRightColumn + 1, previousRightColumn, previousTopRowClamped, previousBottomRowClamped);
            createAndPositionItems(currentLeftColumn, previousLeftColumn - 1, previousTopRowClamped, previousBottomRowClamped, doBuffer);
        }

        if (overlapsAbove) {
            qCDebug(lcItemViewDelegateLifecycle) << "releasing items outside view at the top (including corners), and padding out with new rows at the bottom";
            releaseItems(previousLeftColumn, previousRightColumn, previousTopRow, currentTopRow - 1);
            createAndPositionItems(currentLeftColumn, currentRightColumn, previousBottomRow + 1, currentBottomRow, doBuffer);
        }

        if (overlapsBelow) {
            qCDebug(lcItemViewDelegateLifecycle) << "releasing items outside view at the bottom (including corners), and padding out with new rows at the top";
            releaseItems(previousLeftColumn, previousRightColumn, currentBottomRow + 1, previousBottomRow);
            createAndPositionItems(currentLeftColumn, currentRightColumn, currentTopRow, previousTopRow - 1, doBuffer);
        }
    }

    prevContentPos = contentPos;

    // ### TODO: Don't always return true
    return true;
}

bool QQuickTableViewPrivate::removeNonVisibleItems(const QPointF &fillFrom, const QPointF &fillTo,
                                                   const QPointF &bufferFrom, const QPointF &bufferTo)
{
    Q_UNUSED(bufferFrom);
    Q_UNUSED(bufferTo);
    return false;
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
