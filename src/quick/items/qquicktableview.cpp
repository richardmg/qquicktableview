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
    QQmlIncubator::IncubationMode incubationMode = QQmlIncubator::AsynchronousIfNested;
};

QDebug operator<<(QDebug dbg, const TableSectionLoadRequest request) {
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "TableSectionLoadRequest(";
    dbg << "start:" << request.startCell;
    dbg << " direction:" << request.fillDirection;
    dbg << " incubation:" << request.incubationMode;
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
    QPoint topLeft;
    QPoint bottomRight;
    QPoint visibleBottomRight;
    QRectF visibleBottomRightRect;

    int rowCount;
    int columnCount;

    QQuickTableView::Orientation orientation;
    qreal rowSpacing;
    qreal columnSpacing;
    QQueue<TableSectionLoadRequest> loadRequests;

    QSizeF accurateContentSize;
    bool contentWidthSetExplicit;
    bool contentHeightSetExplicit;

    bool modified;
    bool itemsUnloaded;
    bool loadTableFromScratch;
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

    bool setTopLeftCoord(const QPoint &cellCoord);
    bool setBottomRightCoord(const QPoint &cellCoord);

    int rowAtIndex(int index) const;
    int columnAtIndex(int index) const;
    int indexAt(const QPoint &cellCoord) const;

    inline QPoint coordAt(int index) const;
    inline QPoint coordAt(const FxTableItemSG *tableItem) const;
    inline QPoint topRight() const;
    inline QPoint bottomLeft() const;

    FxTableItemSG *visibleTableItem(int modelIndex) const;
    inline FxTableItemSG *visibleTableItem(const QPoint &cellCoord) const;
    inline FxTableItemSG *topRightItem() const;
    inline FxTableItemSG *bottomLeftItem() const;
    inline FxTableItemSG *itemNextTo(const FxTableItemSG *fxViewItem, const QPoint &direction) const;
    inline FxTableItemSG *edgeItem(const FxTableItemSG *fxTableItem, Qt::Orientation orientation) const;

    inline bool isEdgeItem(const FxTableItemSG *fxTableItem, Qt::Orientation orientation) const;

    FxTableItemSG *loadTableItem(const QPoint &cellCoord, QQmlIncubator::IncubationMode incubationMode);
    inline void showTableItem(FxTableItemSG *fxViewItem);

    bool canHaveMoreItemsInDirection(const QPoint &cellCoord, const QPoint &direction, const QRectF fillRect) const;
    qreal calculateItemX(const FxTableItemSG *fxTableItem) const;
    qreal calculateItemY(const FxTableItemSG *fxTableItem) const;
    qreal calculateItemWidth(const FxTableItemSG *fxTableItem) const;
    qreal calculateItemHeight(const FxTableItemSG *fxTableItem) const;

    void calculateItemGeometry(FxTableItemSG *fxTableItem);
    void calculateContentSize(const FxTableItemSG *fxTableItem);
    void calculateTopLeftAndBottomRightCoords(const FxTableItemSG *fxTableItem);

    void loadInitialItems();
    void refillItemsInsideRect(const QRectF &fillRect, QQmlIncubator::IncubationMode incubationMode);
    void unloadItemsOutsideRect(const QRectF &rect);
    void unloadItems(const QPoint &fromCell, const QPoint &toCell);

    inline void enqueueLoadRequest(const TableSectionLoadRequest &request);
    inline void dequeueCurrentLoadRequest();
    inline bool processCurrentLoadRequest(FxTableItemSG *loadedItem);
    void processLoadRequests(FxTableItemSG *loadedItem);
    bool loadTableItemsOneByOne(TableSectionLoadRequest &request, FxTableItemSG *loadedItem);
    void insertItemIntoTable(FxTableItemSG *fxTableItem);

    inline QString indexToString(int index) const;
    inline QString itemToString(const FxTableItemSG *tableItem) const;
    inline QString tableGeometryToString() const;

    void viewportMoved();

    void dumpTable() const;
    inline void dumpTableGeometry();
};

QQuickTableViewPrivate::QQuickTableViewPrivate()
    : topLeft(kNullCell)
    , bottomRight(kNullCell)
    , visibleBottomRight(kNullCell)
    , visibleBottomRightRect(QRectF())
    , rowCount(-1)
    , columnCount(-1)
    , orientation(QQuickTableView::Vertical)
    , rowSpacing(0)
    , columnSpacing(0)
    , loadRequests(QQueue<TableSectionLoadRequest>())
    , accurateContentSize(QSizeF(-1, -1))
    , contentWidthSetExplicit(false)
    , contentHeightSetExplicit(false)
    , modified(false)
    , itemsUnloaded(false)
    , loadTableFromScratch(true)
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

bool QQuickTableViewPrivate::setTopLeftCoord(const QPoint &cellCoord)
{
    // Refuse to set new top left if has not yet been loaded. This can e.g
    // happen when flicking quickly back and forth, because then new rows
    // and columns that are scrolled into view can still be loading async.
    // By refusing to set an unloaded item as top left, we also signal that
    // the row and column that contains the current top left cannot currently
    // be unloaded. It's needed for layouting purposes until new items are
    // loaded and positioned.
    if (!visibleTableItem(cellCoord))
        return false;

    topLeft = cellCoord;
    return true;
}

bool QQuickTableViewPrivate::setBottomRightCoord(const QPoint &cellCoord)
{
    // Ensure that new bottom right is equal to, or in front of, top-left
    int maxX = qMax(cellCoord.x(), topLeft.x());
    int maxY = qMax(cellCoord.y(), topLeft.y());
    QPoint newBottomRight = QPoint(maxX, maxY);
    if (newBottomRight == bottomRight)
        return false;

    bottomRight = newBottomRight;
    return true;
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

QPoint QQuickTableViewPrivate::topRight() const
{
    return QPoint(bottomRight.x(), topLeft.y());
}

QPoint QQuickTableViewPrivate::bottomLeft() const
{
    return QPoint(topLeft.x(), bottomRight.y());
}

FxTableItemSG *QQuickTableViewPrivate::visibleTableItem(int modelIndex) const
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

FxTableItemSG *QQuickTableViewPrivate::visibleTableItem(const QPoint &cellCoord) const
{
    return visibleTableItem(indexAt(cellCoord));
}

FxTableItemSG *QQuickTableViewPrivate::topRightItem() const
{
    return visibleTableItem(topRight());
}

FxTableItemSG *QQuickTableViewPrivate::bottomLeftItem() const
{
    return visibleTableItem(bottomLeft());
}

FxTableItemSG *QQuickTableViewPrivate::edgeItem(const FxTableItemSG *fxTableItem, Qt::Orientation orientation) const
{
    return orientation == Qt::Horizontal
        ? visibleTableItem(QPoint(coordAt(fxTableItem).x(), topLeft.y()))
        : visibleTableItem(QPoint(topLeft.x(), coordAt(fxTableItem).y()));
}

bool QQuickTableViewPrivate::isEdgeItem(const FxTableItemSG *fxTableItem, Qt::Orientation orientation) const
{
    // An edge item is an item that is on the same row or column as the top left item. But
    // depending on the information needed, we sometimes don't consider an item as en edge
    // item horizontally when laying out vertically and vica versa.
    return orientation == Qt::Vertical
            ? coordAt(fxTableItem).x() == topLeft.x()
            : coordAt(fxTableItem).y() == topLeft.y();
}

void QQuickTableViewPrivate::calculateContentSize(const FxTableItemSG *fxTableItem)
{
    Q_Q(QQuickTableView);

    if (!contentWidthSetExplicit && accurateContentSize.width() == -1) {
        if (bottomRight.x() == columnCount - 1) {
            // We are at the end, and can determine accurate content width
            if (const FxTableItemSG *bottomRightItem = visibleTableItem(bottomRight)) {
                accurateContentSize.setWidth(bottomRightItem->rect().right());
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
        if (bottomRight.y() == rowCount - 1) {
            if (const FxTableItemSG *bottomRightItem = visibleTableItem(bottomRight)) {
                accurateContentSize.setHeight(bottomRightItem->rect().bottom());
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

void QQuickTableViewPrivate::calculateTopLeftAndBottomRightCoords(const FxTableItemSG *fxTableItem)
{
    QPoint addedCoord = coordAt(fxTableItem);

    if (topLeft == kNullCell) {
        setTopLeftCoord(addedCoord);
    } else {
        int minX = qMin(addedCoord.x(), topLeft.x());
        int minY = qMin(addedCoord.y(), topLeft.y());
        setTopLeftCoord(QPoint(minX, minY));
    }

    if (bottomRight == kNullCell) {
        setBottomRightCoord(addedCoord);
    } else {
        int maxX = qMax(addedCoord.x(), bottomRight.x());
        int maxY = qMax(addedCoord.y(), bottomRight.y());
        setBottomRightCoord(QPoint(maxX, maxY));
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

QString QQuickTableViewPrivate::indexToString(int index) const
{
    if (index == kNullValue)
        return QLatin1String("null index");
    return QString::fromLatin1("index: %1 (x:%2, y:%3)").arg(index).arg(columnAtIndex(index)).arg(rowAtIndex(index));
}

QString QQuickTableViewPrivate::itemToString(const FxTableItemSG *tableItem) const
{
    return indexToString(tableItem->index);
}

QString QQuickTableViewPrivate::tableGeometryToString() const
{
    return QString(QLatin1String("count: %1, (%2,%3) -> (%4,%5)"))
            .arg(visibleItems.count())
            .arg(topLeft.x()).arg(topLeft.y())
            .arg(bottomRight.x()).arg(bottomRight.y());
}

void QQuickTableViewPrivate::dumpTable() const
{
    qDebug() << "******* TABLE DUMP *******";
    auto listCopy = visibleItems;
    std::stable_sort(listCopy.begin(), listCopy.end(), [](const FxViewItem *lhs, const FxViewItem *rhs) { return lhs->index < rhs->index; });

    for (int i = 0; i < listCopy.count(); ++i) {
        FxTableItemSG *item = static_cast<FxTableItemSG *>(listCopy.at(i));
        qDebug() << itemToString(item);
    }

    qDebug() << tableGeometryToString();

    QString filename = QStringLiteral("qquicktableview_dumptable_capture.png");
    if (q_func()->window()->grabWindow().save(filename))
        qDebug() << "Window capture saved to:" << filename;
}

void QQuickTableViewPrivate::dumpTableGeometry()
{
    qCDebug(lcTableViewLayout()) << tableGeometryToString();
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
        // This can happen if the item was cached by the model from before.
        qCDebug(lcItemViewDelegateLifecycle) << "item received synchronously:" << cellCoord;
    }

    return loadedItem;
}

void QQuickTableViewPrivate::unloadItems(const QPoint &fromCell, const QPoint &toCell)
{
    qCDebug(lcItemViewDelegateLifecycle) << fromCell << "->" << toCell;
    modified = true;
    itemsUnloaded = true;

    for (int y = fromCell.y(); y <= toCell.y(); ++y) {
        for (int x = fromCell.x(); x <= toCell.x(); ++x) {
            FxTableItemSG *item = visibleTableItem(QPoint(x ,y));
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
    return visibleTableItem(coordAt(fxViewItem) + direction);
}

bool QQuickTableViewPrivate::canHaveMoreItemsInDirection(const QPoint &cellCoord, const QPoint &direction, const QRectF fillRect) const
{
    if (direction.isNull())
        return false;

    Q_TABLEVIEW_ASSERT(visibleTableItem(cellCoord), cellCoord);
    const QRectF itemRect = visibleTableItem(cellCoord)->rect();
    const qreal wholePixelMargin = -1.0;

    if (direction == kRight) {
        if (cellCoord.x() == columnCount - 1)
            return false;
        return itemRect.topRight().x() - fillRect.topRight().x() < wholePixelMargin;
    } else if (direction == kLeft) {
        if (cellCoord.x() == 0)
            return false;
        return fillRect.topLeft().x() - itemRect.topLeft().x() < wholePixelMargin;
    } else if (direction == kDown) {
        if (cellCoord.y() == rowCount - 1)
            return false;
        return itemRect.bottomLeft().y() - fillRect.bottomLeft().y() < wholePixelMargin;
    } else if (direction == kUp) {
        if (cellCoord.y() == 0)
            return false;
        return fillRect.topLeft().y() - itemRect.topLeft().y() < wholePixelMargin;
    } else {
        Q_TABLEVIEW_UNREACHABLE(cellCoord << direction);
    }

    return false;
}

qreal QQuickTableViewPrivate::calculateItemX(const FxTableItemSG *fxTableItem) const
{
    // Calculate the table position x of fxViewItem based on the position of the item on
    // the horizontal edge, or otherwise the item directly next to it. This assumes that at
    // least the item next to it has already been loaded. If the edge item has been
    // loaded, we use that as reference instead, as it allows us to load several items in parallel.
    if (visibleItems.size() == 1) {
        // Special case: the first item will be at 0, 0 for now
        return 0;
    }

    if (isEdgeItem(fxTableItem, Qt::Horizontal)) {
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
        auto edge = edgeItem(fxTableItem, Qt::Horizontal);
        Q_TABLEVIEW_ASSERT(edge, itemToString(fxTableItem));
        return edge->rect().x();
    }
}

qreal QQuickTableViewPrivate::calculateItemY(const FxTableItemSG *fxTableItem) const
{
    // Calculate the table position y of fxViewItem based on the position of the item on
    // the vertical edge, or otherwise the item directly next to it. This assumes that at
    // least the item next to it has already been loaded. If the edge item has been
    // loaded, we use that as reference instead, as it allows us to load several items in parallel.
    if (visibleItems.size() == 1) {
        // Special case: the first item will be at 0, 0 for now
        return 0;
    }

    if (isEdgeItem(fxTableItem, Qt::Vertical)) {
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
        auto edge = edgeItem(fxTableItem, Qt::Vertical);
        Q_TABLEVIEW_ASSERT(edge, itemToString(fxTableItem));
        return edge->rect().y();
    }
}

qreal QQuickTableViewPrivate::calculateItemWidth(const FxTableItemSG *fxTableItem) const
{
    if (visibleItems.size() == 1) {
        return fxTableItem->rect().width();
    }

    if (isEdgeItem(fxTableItem, Qt::Horizontal)) {
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kUp))
            return neighborItem->rect().width();
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kDown))
            return neighborItem->rect().width();
        return fxTableItem->rect().width();
    } else {
        auto edge = edgeItem(fxTableItem, Qt::Horizontal);
        Q_TABLEVIEW_ASSERT(edge, itemToString(fxTableItem));
        return edge->rect().width();
    }
}

qreal QQuickTableViewPrivate::calculateItemHeight(const FxTableItemSG *fxTableItem) const
{
    if (visibleItems.size() == 1) {
        return fxTableItem->rect().height();
    }

    if (isEdgeItem(fxTableItem, Qt::Vertical)) {
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kLeft))
            return neighborItem->rect().height();
        if (FxTableItemSG *neighborItem = itemNextTo(fxTableItem, kRight))
            return neighborItem->rect().height();
        return fxTableItem->rect().height();
    } else {
        auto edge = edgeItem(fxTableItem, Qt::Vertical);
        Q_TABLEVIEW_ASSERT(edge, itemToString(fxTableItem));
        return edge->rect().height();
    }
}

void QQuickTableViewPrivate::calculateItemGeometry(FxTableItemSG *fxTableItem)
{
    qreal w = calculateItemWidth(fxTableItem);
    qreal h = calculateItemHeight(fxTableItem);
    fxTableItem->setSize(QSizeF(w, h), true);

    qreal x = calculateItemX(fxTableItem);
    qreal y = calculateItemY(fxTableItem);
    fxTableItem->setPosition(QPointF(x, y));

    qCDebug(lcItemViewDelegateLifecycle()) << itemToString(fxTableItem) << fxTableItem->rect();
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

    qCDebug(lcItemViewDelegateLifecycle) << "loaded asynchronously:" << d->indexToString(index);

    // It's important to use createItem to get the item, and
    // not the object argument, since the former will ref-count it.
    FxTableItemSG *item = static_cast<FxTableItemSG *>(d->createItem(index, false));

    Q_ASSERT(item);
    Q_ASSERT(item->item);

    d->processLoadRequests(item);
}

void QQuickTableViewPrivate::insertItemIntoTable(FxTableItemSG *fxTableItem)
{
    qCDebug(lcItemViewDelegateLifecycle) << itemToString(fxTableItem);
    modified = true;
    visibleItems.append(fxTableItem);

    calculateTopLeftAndBottomRightCoords(fxTableItem);
    calculateItemGeometry(fxTableItem);
    calculateContentSize(fxTableItem);

    QPoint addedCoord = coordAt(fxTableItem);

    if (auto bottomRightItem = visibleTableItem(bottomRight)) {
        QRectF bottomRightRect = bottomRightItem->rect();
        if (viewportRect().intersects(bottomRightRect)) {
            visibleBottomRight = addedCoord;
            visibleBottomRightRect = bottomRightRect;
            qDebug() << "visibleBottomRightRect:" << visibleBottomRightRect;
        }
    }

    showTableItem(fxTableItem);
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

bool QQuickTableViewPrivate::processCurrentLoadRequest(FxTableItemSG *loadedItem)
{
    TableSectionLoadRequest &request = loadRequests.head();
    QPoint horizontalFillDirection = QPoint(request.fillDirection.x(), 0);
    QPoint verticalFillDirection = QPoint(0, request.fillDirection.y());
    bool loadHorizontal = !horizontalFillDirection.isNull();
    bool loadVertical = !verticalFillDirection.isNull();

    if (!loadedItem) {
        // (Re)load the next item in the request
        request.requestedCell = request.requestedCell == kNullCell ? request.startCell : request.requestedCell;
        loadedItem = loadTableItem(request.requestedCell, request.incubationMode);
    }

    while (loadedItem) {
        insertItemIntoTable(loadedItem);

        // Continue loading items in the request (first horizontally, then
        // vertically), until one of them ends up loading async. If so, we just return, and
        // wait for this function to be called again once the requested item is ready.
        const QRect tableRect = QRect(topLeft, bottomRight);
        const QPoint loadedCoord = coordAt(loadedItem);

        if (loadHorizontal && tableRect.contains(loadedCoord + horizontalFillDirection))
            request.requestedCell = loadedCoord + horizontalFillDirection;
        else if (loadVertical && tableRect.contains(loadedCoord + verticalFillDirection))
            request.requestedCell = QPoint(request.startCell.x(), loadedCoord.y() + 1);
        else
            return true;

        loadedItem = loadTableItem(request.requestedCell, request.incubationMode);
    }

    return false;
}

void QQuickTableViewPrivate::processLoadRequests(FxTableItemSG *loadedItem)
{
    // This function will continue processing the current load request, which is the
    // one that is head in the loadRequests que. We load as many items as possible, and
    // handle as many pending load requests we can, before we leave. If one of the items
    // we try to load ends up loading async, we need to wait for it before we can continue.
    // In that case we just leave, and continue the process later, once we receive it.
    // This function will then be called again, but now with loadedItem pointing to the
    // asynchronously loaded item. It is also fine to change the incubation mode of the
    // load request and call this function again while we're waiting for an async item, in
    // case something has changed, and we cannot affort to wait anymore.

    forever {
        if (loadRequests.isEmpty()) {
            // We have nothing more load requests pending, but check if we
            // the view port moved since we started processing the first
            // load requests, and if so, add or remove items at the edges.
            // This might cause new load requests to be queued.
            QRectF visibleRect = viewportRect();
            QRectF bufferRect = visibleRect.adjusted(-buffer, -buffer, buffer, buffer);

            unloadItemsOutsideRect(bufferRect);
            refillItemsInsideRect(visibleRect, QQmlIncubator::AsynchronousIfNested);

            if (buffer > 0 && loadRequests.isEmpty()) {
                // There is no visible part of the view that is not covered with items, so
                // we can spend time loading items into buffer for quick flick response later.
                refillItemsInsideRect(bufferRect, QQmlIncubator::Asynchronous);
            }

            if (loadRequests.isEmpty())
                return;
        }

        if (!processCurrentLoadRequest(loadedItem))
            return;

        loadedItem = nullptr;
        dequeueCurrentLoadRequest();
        dumpTableGeometry();
    }
}

void QQuickTableViewPrivate::unloadItemsOutsideRect(const QRectF &rect)
{
    do {
        itemsUnloaded = false;

        Q_TABLEVIEW_ASSERT(visibleTableItem(topLeft), rect);
        Q_TABLEVIEW_ASSERT(visibleTableItem(bottomRight), rect);

        const QRectF &topLeftRect = visibleTableItem(topLeft)->rect();
        const QRectF &bottomRightRect = visibleTableItem(bottomRight)->rect();
        const qreal wholePixelMargin = -1.0;

        if (topLeftRect.right() - rect.left() < wholePixelMargin) {
            QPoint from = topLeft;
            QPoint to = bottomLeft();
            if (setTopLeftCoord(topLeft + kRight)) {
                qCDebug(lcTableViewLayout()) << "unload left column" << from.x();
                unloadItems(from, to);
            }
        } else if (rect.right() - bottomRightRect.left() < wholePixelMargin) {
            QPoint from = topRight();
            QPoint to = bottomRight;
            if (setBottomRightCoord(bottomRight + kLeft)) {
                qCDebug(lcTableViewLayout()) << "unload right column" << from.x();
                unloadItems(from, to);
            }
        }

        if (topLeftRect.bottom() - rect.top() < wholePixelMargin) {
            QPoint from = topLeft;
            QPoint to = topRight();
            if (setTopLeftCoord(topLeft + kDown)) {
                qCDebug(lcTableViewLayout()) << "unload top row" << from.y();
                unloadItems(from, to);
            }
        } else if (rect.bottom() - bottomRightRect.top() < wholePixelMargin) {
            QPoint from = bottomLeft();
            QPoint to = bottomRight;
            if (setBottomRightCoord(bottomRight + kUp)) {
                qCDebug(lcTableViewLayout()) << "unload bottom row" << from.y();
                unloadItems(from, to);
            }
        }

        if (itemsUnloaded)
            dumpTableGeometry();

    } while (itemsUnloaded);
}

void QQuickTableViewPrivate::loadInitialItems()
{
    // Load top-left item. After loaded, processLoadRequest() will take
    // care of filling out the rest of the table.
    Q_TABLEVIEW_ASSERT(visibleItems.isEmpty(), visibleItems.count());
    Q_TABLEVIEW_ASSERT(topLeft == kNullCell, topLeft);

    QPoint topLeftCoord(0, 0);

    TableSectionLoadRequest requestTopLeftItem;
    requestTopLeftItem.startCell = topLeftCoord;
    qCDebug(lcTableViewLayout()) << "load top-left:" << requestTopLeftItem.startCell;
    enqueueLoadRequest(requestTopLeftItem);
}

void QQuickTableViewPrivate::refillItemsInsideRect(const QRectF &fillRect, QQmlIncubator::IncubationMode incubationMode)
{
    if (canHaveMoreItemsInDirection(topLeft, kLeft, fillRect)) {
        TableSectionLoadRequest request;
        request.startCell = topLeft + kLeft;
        request.fillDirection = kDown;
        request.incubationMode = incubationMode;
        qCDebug(lcTableViewLayout()) << "load left column" << request.startCell.x();
        enqueueLoadRequest(request);
    } else if (canHaveMoreItemsInDirection(topRight(), kRight, fillRect)) {
        TableSectionLoadRequest request;
        request.startCell = topRight() + kRight;
        request.fillDirection = kDown;
        request.incubationMode = incubationMode;
        qCDebug(lcTableViewLayout()) << "load right column" << request.startCell.x();
        enqueueLoadRequest(request);
    } else if (canHaveMoreItemsInDirection(topLeft, kUp, fillRect)) {
        TableSectionLoadRequest request;
        request.startCell = topLeft + kUp;
        request.fillDirection = kRight;
        request.incubationMode = incubationMode;
        qCDebug(lcTableViewLayout()) << "load top row" << request.startCell.y();
        enqueueLoadRequest(request);
    } else if (canHaveMoreItemsInDirection(bottomLeft(), kDown, fillRect)) {
        TableSectionLoadRequest request;
        request.startCell = QPoint(topLeft.x(), bottomLeft().y()) + kDown;
        request.fillDirection = kRight;
        request.incubationMode = incubationMode;
        qCDebug(lcTableViewLayout()) << "load bottom row" << request.startCell.y();
        enqueueLoadRequest(request);
    }
}

bool QQuickTableViewPrivate::addRemoveVisibleItems()
{
    if (!viewportRect().isValid())
        return false;

    if (loadTableFromScratch) {
        loadTableFromScratch = false;
        loadInitialItems();
    } else if (!loadRequests.isEmpty()) {
        // When a new flick starts, we fast-forward loading of the current
        // load request in case it loads to buffer

        //TODO: if we're here, doesn't it always mean that the current request
        // is loading async (is asyncIfnested possible? meaning, is it possible
        // to start a flick if we're still inside an async loader?)

        for (TableSectionLoadRequest &request : loadRequests) {
            if (request.incubationMode == QQmlIncubator::Asynchronous) {
                qDebug() << "speedup:" << request;
                request.incubationMode = QQmlIncubator::AsynchronousIfNested;
            }
        }
    }

    modified = false;

    processLoadRequests(nullptr);

    return modified;
}

QQuickTableView::QQuickTableView(QQuickItem *parent)
    : QQuickAbstractItemView(*(new QQuickTableViewPrivate), parent)
{
}

void QQuickTableViewPrivate::viewportMoved()
{
    Q_Q(const QQuickTableView);

    QPoint newVisibleBottomRight = visibleBottomRight;

    if (viewportRect().right() > visibleBottomRightRect.right()) {
        // Bottom-right completely inside visible area on the right
        newVisibleBottomRight += kRight;
    } else if (viewportRect().right() < visibleBottomRightRect.left()) {
        // Bottom-right completly outside visible area on the right
        newVisibleBottomRight += kLeft;
    }

    if (viewportRect().bottom() > visibleBottomRightRect.bottom()) {
        // Bottom-right completely inside visible area at the bottom
        newVisibleBottomRight += kDown;
    } else if (viewportRect().bottom() < visibleBottomRightRect.top()) {
        // Bottom-right completly outside visible area at the bottom
        newVisibleBottomRight += kUp;
    }

    if (newVisibleBottomRight != visibleBottomRight) {
        if (auto newVisibleBottomRightItem = visibleTableItem(newVisibleBottomRight)) {
            visibleBottomRight = newVisibleBottomRight;
            visibleBottomRightRect = newVisibleBottomRightItem->rect();
            qDebug() << "new visible bottom right:" << newVisibleBottomRight;
        }
    }

    addRemoveVisibleItems();
}

void QQuickTableView::viewportMoved(Qt::Orientations orient)
{
    QQuickAbstractItemView::viewportMoved(orient);
    d_func()->viewportMoved();
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

    // NB: deliberatly skipping QQuickAbstractItemView, since it does so
    // many wierd things that I don't need or understand. That code is messy...
    QQuickFlickable::componentComplete();
}

QT_END_NAMESPACE
