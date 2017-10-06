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
    void updateAverageSize();

    QSizeF size() const;
    QPointF position() const;
    QPointF startPosition() const;
    QPointF originPosition() const;
    QPointF endPosition() const;
    QPointF lastPosition() const;

    QPointF itemPosition(int row, int column) const;
    QPointF itemPosition(FxViewItem *item) const;
    QPointF itemEndPosition(FxViewItem *item) const;
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

    int rows;
    int columns;
    int visibleRows;
    int visibleColumns;
    QQuickTableView::Orientation orientation;
    qreal rowSpacing;
    qreal columnSpacing;
    QPointF visiblePos;
    QSizeF averageSize;

    void debug_removeAllItems();

protected:
    bool addVisibleItems(const QPointF &fillFrom, const QPointF &fillTo,
                         const QPointF &bufferFrom, const QPointF &bufferTo, bool doBuffer);
    bool removeNonVisibleItems(const QPointF &fillFrom, const QPointF &fillTo,
                               const QPointF &bufferFrom, const QPointF &bufferTo);
    void createAndPositionItem(int row, int col, bool doBuffer);
};

QQuickTableViewPrivate::QQuickTableViewPrivate()
    : rows(-1),
      columns(-1),
      visibleRows(0),
      visibleColumns(0),
      orientation(QQuickTableView::Vertical),
      rowSpacing(0),
      columnSpacing(0)
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

int QQuickTableViewPrivate::columnAtPos(qreal x) const
{
    // ### TODO: for now we assume all columns has the same width. This will not be the case in the end.
    // The strategy should either be to keep all the column widths in a separate array, or
    // inspect the current visible items to make a guess.
    return x / (columnWidth(0) + columnSpacing);
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
    int visibleRow = rowAtIndex(visibleIndex);
    int visibleColumn = columnAtIndex(visibleIndex);
    if (row < visibleRow || row >= visibleRow + visibleRows
            || column < visibleColumn || column >= visibleRow + visibleRows)
        return nullptr;

    int r = row - visibleRow;
    int c = column - visibleColumn;
    return visibleItems.value(r + c * visibleRows);
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

qreal QQuickTableViewPrivate::columnPos(int column) const
{
    // ### TODO: Support columns having different widths. Should this be stored in a separate list as well?
    return column * (columnWidth(column) + columnSpacing);

//    // ### TODO: right-to-left
//    int visibleRow = rowAt(visibleIndex);
//    FxViewItem *item = visibleItemAt(visibleRow, column);
//    if (item)
//        return item->itemX();

//    // estimate
//    int visibleColumn = columnAt(visibleIndex);
//    if (column < visibleColumn) {
//        int count = visibleColumn - column;
//        FxViewItem *topLeft = visibleItemAt(visibleRow, visibleColumn);
//        return topLeft->itemX() - count * averageSize.width(); // ### TODO: spacing
//    } else if (column > visibleColumn + visibleColumns) {
//        int count = column - visibleColumn - visibleColumns;
//        FxViewItem *topRight = visibleItemAt(visibleRow, visibleColumn + visibleColumns);
//        return topRight->itemX() + topRight->itemWidth() + count * averageSize.width(); // ### TODO: spacing
//    }
//    return 0;
}

qreal QQuickTableViewPrivate::columnWidth(int column) const
{
    // ### TODO: Rather than search through visible items, I think this information should be
    // specified more explixit, e.g in an separate list. Use a constant for now.
    Q_UNUSED(column);
    return 120;

//    int row = rowAt(visibleIndex);
//    FxViewItem *item = visibleItemAt(row, column);
//    return item ? item->itemWidth() : averageSize.width();
}

void QQuickTableViewPrivate::updateAverageSize()
{
    if (visibleItems.isEmpty())
        return;

    QSizeF sum;
    for (FxViewItem *item : qAsConst(visibleItems))
        sum += QSizeF(item->itemWidth(), item->itemHeight());

    averageSize.setWidth(qRound(sum.width() / visibleItems.count()));
    averageSize.setHeight(qRound(sum.height() / visibleItems.count()));
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

QPointF QQuickTableViewPrivate::startPosition() const
{
    return isContentFlowReversed() ? -lastPosition() : originPosition();
}

QPointF QQuickTableViewPrivate::originPosition() const
{
    return QPointF(0, 0);
}

QPointF QQuickTableViewPrivate::endPosition() const
{
    return isContentFlowReversed() ? -originPosition() : lastPosition();
}

QPointF QQuickTableViewPrivate::lastPosition() const
{
    int lastRow = rows - 1;
    int lastColumn = columns - 1;
    return QPointF(columnPos(lastColumn) + columnWidth(lastColumn), rowPos(lastRow) + rowHeight(lastRow));
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

void QQuickTableViewPrivate::updateViewport()
{
    Q_Q(QQuickTableView);
    QSizeF size;
    if (isValid() || !visibleItems.isEmpty()) {
        QPointF start = startPosition();
        QPointF end = endPosition();
        size.setWidth(end.x() - start.x());
        size.setHeight(end.y() - start.y());
    }
    q->setContentWidth(size.width());
    q->setContentHeight(size.height());
}

static QPointF operator+(const QPointF &pos, const QSizeF &size)
{
    return QPointF(pos.x() + size.width(), pos.y() + size.height());
}

static QPointF operator-(const QPointF &pos, const QSizeF &size)
{
    return QPointF(pos.x() + size.width(), pos.y() + size.height());
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
//    if (!visibleItems.isEmpty())
//        visiblePos = itemPosition(*visibleItems.constBegin());
    updateAverageSize();
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
    if (!item)
        return;

    QPointF itemPos = itemPosition(row, col);
    if (!transitioner || !transitioner->canTransition(QQuickItemViewTransitioner::PopulateTransition, true))
        item->setPosition(itemPos, true);

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
    Q_UNUSED(bufferFrom);
    Q_UNUSED(bufferTo);

    if ((fillFrom - fillTo).isNull())
        return false;

    if (true) {
        // Remove all items each time for now
        for (FxViewItem *item : visibleItems)
            releaseItem(item);

        releaseItem(currentItem);
        currentItem = 0;
        visibleItems.clear();
    }

//    Q_Q(QQuickTableView);

    // For simplicity, we assume that we always specify the number of rows and columns directly
    // in TableView. But we should also allow those properties to be unspecified, and if so, get
    // the counts from the model instead. In addition, the counts might change inbetween two calls
    // to this function, which might cause previousBottomRow and previousRightColumn to end up wrong!
    // So we should probably store the last values used to be certain.
    int rowCount = rows;
    int columnCount = columns;

    QPointF previousVisiblePos = visiblePos;
    int previousBottomRow = qMin(rowAtPos(previousVisiblePos.y() + height), rowCount - 1);
    int previousRightColumn = qMin(columnAtPos(previousVisiblePos.x() + width), columnCount - 1);

    visiblePos = fillFrom;
    int currentTopRow = rowAtPos(visiblePos.y());
    int currentBottomRow = qMin(rowAtPos(visiblePos.y() + height), rowCount - 1);
    int currentLeftColumn = columnAtPos(visiblePos.x());
    int currentRightColumn = qMin(columnAtPos(visiblePos.x() + width), columnCount - 1);

    if (visibleItems.isEmpty()) {
        // Fill the whole table
        previousBottomRow = currentTopRow - 1;
        previousRightColumn = currentLeftColumn - 1;
    }

    qCDebug(lcItemViewDelegateLifecycle) << "\n\tfrom:" << fillFrom
                                         << "to:" << fillTo
                                         << "\n\tbufferFrom:" << bufferFrom
                                         << "bufferTo:" << bufferFrom
                                         << "\n\tpreviousBottomRow:" << previousBottomRow
                                         << "currentBottomRow:" << currentBottomRow
                                         << "\n\tpreviousRightColumn:" << previousRightColumn
                                         << "currentRightColumn:" << currentRightColumn;

    // Fill in missing columns for already existsing rows
    for (int row = currentTopRow; row <= previousBottomRow; ++row) {
        for (int col = previousRightColumn + 1; col <= currentRightColumn; ++col) {
            createAndPositionItem(row, col, doBuffer);
        }
    }

    // Fill in missing rows at the bottom
    for (int row = previousBottomRow + 1; row <= currentBottomRow; ++row) {
        for (int col = currentLeftColumn; col <= currentRightColumn; ++col) {
            createAndPositionItem(row, col, doBuffer);
        }
    }

    // Next:
    // - fill top and left.
    // - don't refill a cell if the item for that cell has already been created

    // ### TODO: Handle items that should be prepended
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
    Q_D(const QQuickTableView);
    if (QQmlDelegateModel *delegateModel = qobject_cast<QQmlDelegateModel *>(d->model.data()))
        return delegateModel->columns();
    return qMax(1, d->columns);
}

void QQuickTableView::setColumns(int columns)
{
    Q_D(QQuickTableView);
    if (d->columns == columns)
        return;

    d->columns = columns;
    if (d->componentComplete && d->model && d->ownModel)
        static_cast<QQmlDelegateModel *>(d->model.data())->setColumns(columns);
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
