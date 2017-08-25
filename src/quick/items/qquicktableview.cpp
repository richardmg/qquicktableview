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

    int rowAt(int index) const;
    int columnAt(int index) const;
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

protected:
    bool addVisibleItems(const QPointF &fillFrom, const QPointF &fillTo,
                         const QPointF &bufferFrom, const QPointF &bufferTo, bool doBuffer);
    bool removeNonVisibleItems(const QPointF &bufferFrom, const QPointF &bufferTo);
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

int QQuickTableViewPrivate::rowAt(int index) const
{
    Q_Q(const QQuickTableView);
    int rows = q->rows();
    if (index < 0 || index >= itemCount || rows <= 0)
        return -1;
    return index % rows;
}

int QQuickTableViewPrivate::columnAt(int index) const
{
    Q_Q(const QQuickTableView);
    int rows = q->rows();
    if (index < 0 || index >= itemCount || rows <= 0)
        return -1;
    return index / rows;
}

int QQuickTableViewPrivate::indexAt(int row, int column) const
{
    Q_Q(const QQuickTableView);
    int rows = q->rows();
    int columns = q->columns();
    if (row < 0 || row >= rows || column < 0 || column >= columns)
        return -1;
    return row + column * rows;
}

FxViewItem *QQuickTableViewPrivate::visibleItemAt(int row, int column) const
{
    int visibleRow = rowAt(visibleIndex);
    int visibleColumn = columnAt(visibleIndex);
    if (row < visibleRow || row >= visibleRow + visibleRows
            || column < visibleColumn || column >= visibleRow + visibleRows)
        return nullptr;

    int r = row - visibleRow;
    int c = column - visibleColumn;
    return visibleItems.value(r + c * visibleRows);
}

qreal QQuickTableViewPrivate::rowPos(int row) const
{
    // ### TODO: bottom-to-top
    int visibleColumn = columnAt(visibleIndex);
    FxViewItem *item = visibleItemAt(row, visibleColumn);
    if (item)
        return item->itemY();

    // estimate
    int visibleRow = rowAt(visibleIndex);
    if (row < visibleRow) {
        int count = visibleRow - row;
        FxViewItem *topLeft = visibleItemAt(visibleRow, visibleColumn);
        return topLeft->itemY() - count * averageSize.height(); // ### spacing
    } else if (row > visibleRow + visibleRows) {
        int count = row - visibleRow - visibleRows;
        FxViewItem *bottomLeft = visibleItemAt(visibleRow + visibleRows, visibleColumn);
        return bottomLeft->itemY() + bottomLeft->itemHeight() + count * averageSize.height(); // ### spacing
    }
    return 0;
}

qreal QQuickTableViewPrivate::rowHeight(int row) const
{
    int column = columnAt(visibleIndex);
    FxViewItem *item = visibleItemAt(row, column);
    return item ? item->itemHeight() : averageSize.height();
}

qreal QQuickTableViewPrivate::columnPos(int column) const
{
    // ### TODO: right-to-left
    int visibleRow = rowAt(visibleIndex);
    FxViewItem *item = visibleItemAt(visibleRow, column);
    if (item)
        return item->itemX();

    // estimate
    int visibleColumn = columnAt(visibleIndex);
    if (column < visibleColumn) {
        int count = visibleColumn - column;
        FxViewItem *topLeft = visibleItemAt(visibleRow, visibleColumn);
        return topLeft->itemX() - count * averageSize.width(); // ### TODO: spacing
    } else if (column > visibleColumn + visibleColumns) {
        int count = column - visibleColumn - visibleColumns;
        FxViewItem *topRight = visibleItemAt(visibleRow, visibleColumn + visibleColumns);
        return topRight->itemX() + topRight->itemWidth() + count * averageSize.width(); // ### TODO: spacing
    }
    return 0;
}

qreal QQuickTableViewPrivate::columnWidth(int column) const
{
    int row = rowAt(visibleIndex);
    FxViewItem *item = visibleItemAt(row, column);
    return item ? item->itemWidth() : averageSize.width();
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
    QPointF pos;
    if (!visibleItems.isEmpty()) {
        FxViewItem *item = visibleItems.first();
        pos = QPointF(item->itemX(), item->itemY());
        if (visibleIndex > 0) {
            pos.rx() -= columnAt(visibleIndex) * (averageSize.width() + rowSpacing);
            pos.ry() -= rowAt(visibleIndex) * (averageSize.height() + columnSpacing);
        }
    }
    return pos;
}

QPointF QQuickTableViewPrivate::endPosition() const
{
    return isContentFlowReversed() ? -originPosition() : lastPosition();
}

QPointF QQuickTableViewPrivate::lastPosition() const
{
    Q_Q(const QQuickTableView);
    QPointF pos;
    if (!visibleItems.isEmpty()) {
        int invisibleCount = INT_MIN;
        int delayRemovedCount = 0;
        for (int i = visibleItems.count()-1; i >= 0; --i) {
            FxViewItem *item = visibleItems.at(i);
            if (item->index != -1) {
                // Find the invisible count after the last visible item with known index
                invisibleCount = model->count() - (item->index + 1 + delayRemovedCount);
                break;
            } else if (item->attached->delayRemove()) {
                ++delayRemovedCount;
            }
        }
        if (invisibleCount == INT_MIN) {
            // All visible items are in delayRemove state
            invisibleCount = model->count();
        }
        FxViewItem *item = *(--visibleItems.constEnd());
        pos = QPointF(item->itemX() + item->itemWidth(), item->itemY() + item->itemHeight());
        int columns = columnAt(invisibleCount);
        if (columns > 0)
            pos.rx() += columnAt(invisibleCount) * (averageSize.width() + columnSpacing);
        int rows = rowAt(invisibleCount);
        if (rows > 0)
            pos.ry() += invisibleCount * (averageSize.height() + rowSpacing);
    } else if (model) {
        int columns = q->columns();
        if (columns > 0)
            pos.setX(columns * averageSize.width() + (columns - 1) * columnSpacing);
        int rows = q->rows();
        if (rows > 0)
            pos.setY(rows * averageSize.height() + (rows - 1) * rowSpacing);
    }
    return pos;
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
    bool removed = removeNonVisibleItems(bufferFrom, bufferTo);

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
    if (!visibleItems.isEmpty())
        visiblePos = itemPosition(*visibleItems.constBegin());
    updateAverageSize();
}

FxViewItem *QQuickTableViewPrivate::newViewItem(int index, QQuickItem *item)
{
    Q_Q(QQuickTableView);
    Q_UNUSED(index);
    return new FxTableItemSG(item, q, false);
}

bool QQuickTableViewPrivate::addVisibleItems(const QPointF &fillFrom, const QPointF &fillTo,
                                             const QPointF &bufferFrom, const QPointF &bufferTo, bool doBuffer)
{
    Q_Q(QQuickTableView);
    QPointF itemEnd = visiblePos;
    if (visibleItems.count()) {
        visiblePos = itemPosition(*visibleItems.constBegin());
        itemEnd = itemEndPosition(*(--visibleItems.constEnd())) + QPointF(columnSpacing, rowSpacing);
    }

    int modelIndex = findLastVisibleIndex();


    // findLastVisibleIndex() returnerer dobbelt så mange som faktisk får plass.
    // Det betyr at vi lager opp dobbelt så mange som vi trenger nedenfor!

    qDebug() << "last visible index:" << modelIndex;


    bool haveValidItems = modelIndex >= 0;
    modelIndex = modelIndex < 0 ? visibleIndex : modelIndex + 1;

    if (haveValidItems && (bufferFrom.x() > itemEnd.x() + averageSize.width() + columnSpacing
                           || bufferFrom.y() > itemEnd.y() + averageSize.height() + rowSpacing
                           || bufferTo.x() < visiblePos.x() - averageSize.width() - columnSpacing
                           || bufferTo.y() < visiblePos.y() - averageSize.height() - rowSpacing)) {

        qDebug() << "NEVER HERE!!!!!!";

        // We've jumped more than a page.  Estimate which items are now
        // visible and fill from there.
        int rows = (fillFrom.y() - itemEnd.y()) / (averageSize.height() + rowSpacing);
        int columns = (fillFrom.x() - itemEnd.x()) / (averageSize.width() + columnSpacing);
        int count = rows * q->columns() + columns;
        int newModelIdx = qBound(0, modelIndex + count, model->count());
        count = newModelIdx - modelIndex;
        if (count) {
            for (FxViewItem *item : qAsConst(visibleItems))
                releaseItem(item);
            visibleItems.clear();
            modelIndex = newModelIdx;
            visibleIndex = modelIndex;
            visiblePos = itemEnd + QPointF((averageSize.width() + columnSpacing) * columns,
                                           (averageSize.height() + rowSpacing) * rows);
            itemEnd = visiblePos;
        }
    }

    bool changed = false;
    FxTableItemSG *item = nullptr;
    QPointF pos = itemEnd;

    // It appears that we create to many items in the next loop. Figure out why!
    qDebug() << "fill to:" << fillTo.x() << fillTo.y();

    while (modelIndex < model->count() && (pos.x() <= fillTo.x() || pos.y() <= fillTo.y())) {
        if (!(item = static_cast<FxTableItemSG *>(createItem(modelIndex, doBuffer))))
            break;
        qCDebug(lcItemViewDelegateLifecycle) << "refill: append item" << modelIndex << "pos" << pos << "buffer" << doBuffer << "item" << (QObject *)(item->item);
        if (!transitioner || !transitioner->canTransition(QQuickItemViewTransitioner::PopulateTransition, true)) // pos will be set by layoutVisibleItems()
            item->setPosition(pos, true);
        if (item->item)
            QQuickItemPrivate::get(item->item)->setCulled(doBuffer);
        QSizeF size = itemSize(item);
        pos += QPointF(size.width() + columnSpacing, size.height() + rowSpacing);
        visibleItems.append(item);
        ++modelIndex;
        changed = true;
    }

    if (doBuffer && requestedIndex != -1) // already waiting for an item
        return changed;

    while (visibleIndex > 0 && visibleIndex <= model->count() && (visiblePos.x() > fillFrom.x() || visiblePos.y() > fillFrom.y())) {
        if (!(item = static_cast<FxTableItemSG *>(createItem(visibleIndex-1, doBuffer))))
            break;
        qCDebug(lcItemViewDelegateLifecycle) << "refill: prepend item" << visibleIndex-1 << "current top pos" << visiblePos << "buffer" << doBuffer << "item" << (QObject *)(item->item);
        --visibleIndex;
        QSizeF size = itemSize(item);
        visiblePos -= QPointF(size.width() + columnSpacing, size.height() + rowSpacing);
        if (!transitioner || !transitioner->canTransition(QQuickItemViewTransitioner::PopulateTransition, true)) // pos will be set by layoutVisibleItems()
            item->setPosition(visiblePos, true);
        if (item->item)
            QQuickItemPrivate::get(item->item)->setCulled(doBuffer);
        visibleItems.prepend(item);
        changed = true;
    }

    return changed;
}

bool QQuickTableViewPrivate::removeNonVisibleItems(const QPointF &bufferFrom, const QPointF &bufferTo)
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
