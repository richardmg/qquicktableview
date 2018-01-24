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

#define Q_TABLEVIEW_UNREACHABLE(output) { dumpTable(); qDebug() << "output:" << output; Q_UNREACHABLE(); }
#define Q_TABLEVIEW_ASSERT(cond, output) Q_ASSERT(cond || [&](){ dumpTable(); qDebug() << "output:" << output; return false;}())

static const Qt::Edge allTableEdges[] = { Qt::LeftEdge, Qt::RightEdge, Qt::TopEdge, Qt::BottomEdge };

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
    QLine itemsToLoad;
    Qt::Edge edgeToLoad;
    QQmlIncubator::IncubationMode incubationMode = QQmlIncubator::AsynchronousIfNested;

    int loadIndex;
    int loadCount;
    bool active = false;
};

QString incubationModeToString(QQmlIncubator::IncubationMode mode)
{
    switch (mode) {
    case QQmlIncubator::Asynchronous:
        return QLatin1String("Asynchronous");
    case QQmlIncubator::AsynchronousIfNested:
        return QLatin1String("AsynchronousIfNested");
    case QQmlIncubator::Synchronous:
        return QLatin1String("Synchronous");
    }
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const TableSectionLoadRequest request) {
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "TableSectionLoadRequest(";
    dbg << "edge:" << request.edgeToLoad;
    dbg << " section:" << request.itemsToLoad;
    dbg << " loadIndex:" << request.loadIndex;
    dbg << " loadCount:" << request.loadCount;
    dbg << " incubation:" << incubationModeToString(request.incubationMode);
    dbg << " initialized:" << request.active;
    dbg << ')';
    return dbg;
}
#endif

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
    QRectF loadedTableRect;
    QRectF loadedTableRectInside;

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
    bool hasBufferedItems;
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

    inline int lineLength(const QLine &line);

    int rowAtIndex(int index) const;
    int columnAtIndex(int index) const;
    int indexAt(const QPoint &cellCoord) const;

    inline QPoint coordAt(int index) const;
    inline QPoint coordAt(const FxTableItemSG *tableItem) const;
    inline QPoint coordAt(const QLine &line, int index);

    FxTableItemSG *loadedTableItem(int modelIndex) const;
    inline FxTableItemSG *loadedTableItem(const QPoint &cellCoord) const;
    inline FxTableItemSG *itemNextTo(const FxTableItemSG *fxViewItem, const QPoint &direction) const;
    QLine rectangleEdge(const QRect &rect, Qt::Edge tableEdge);

    QRect expandedRect(const QRect &rect, Qt::Edge edge, int increment);

    FxTableItemSG *loadTableItem(const QPoint &cellCoord, QQmlIncubator::IncubationMode incubationMode);
    inline void showTableItem(FxTableItemSG *fxViewItem);

    bool canLoadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const;
    bool canUnloadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const;

    qreal calculateItemX(const FxTableItemSG *fxTableItem, Qt::Edge tableEdge) const;
    qreal calculateItemY(const FxTableItemSG *fxTableItem, Qt::Edge tableEdge) const;
    qreal calculateItemWidth(const FxTableItemSG *fxTableItem, Qt::Edge tableEdge) const;
    qreal calculateItemHeight(const FxTableItemSG *fxTableItem, Qt::Edge tableEdge) const;

    void calculateItemGeometry(FxTableItemSG *fxTableItem, Qt::Edge tableEdge);
    void calculateContentSize();
    void syncLoadedTableRectFromLoadedTable();
    void syncLoadedTableFromLoadRequest();

    void loadInitialTopLeftItem();
    void loadItemsInsideRect(const QRectF &fillRect, QQmlIncubator::IncubationMode incubationMode);
    void unloadItemsOutsideRect(const QRectF &rect);
    void unloadItem(const QPoint &cell);
    void unloadItems(const QLine &items);

    void unloadBuffer();
    void cancelLoadRequest();
    void processLoadRequest();
    void loadAndUnloadTableItems();
    bool loadTableItemsOneByOne(TableSectionLoadRequest &request, FxTableItemSG *loadedItem);
    void insertItemIntoTable(FxTableItemSG *fxTableItem);

    inline QString tableLayoutToString() const;

    void dumpTable() const;
};

QQuickTableViewPrivate::QQuickTableViewPrivate()
    : loadedTable(QRect())
    , loadedTableRect(QRectF())
    , loadedTableRectInside(QRectF())
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
    , hasBufferedItems(false)
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

int QQuickTableViewPrivate::lineLength(const QLine &line)
{
    // Note: a line is either vertical or horisontal
    return line.x2() - line.x1() + line.y2() - line.y1() + 1;
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

QPoint QQuickTableViewPrivate::coordAt(const QLine &line, int index)
{
    // Note: a line is either vertical or horisontal
    int x = line.p1().x() + (line.dx() ? index : 0);
    int y = line.p1().y() + (line.dy() ? index : 0);
    return QPoint(x, y);
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

void QQuickTableViewPrivate::calculateContentSize()
{
    Q_Q(QQuickTableView);

    const qreal flickSpace = 500;

    if (!contentWidthSetExplicit && accurateContentSize.width() == -1) {
        if (loadedTable.topRight().x() == columnCount - 1)
            q->setContentWidth(loadedTableRect.right());
        else
            q->setContentWidth(loadedTableRect.right() + flickSpace);
    }

    if (!contentHeightSetExplicit && accurateContentSize.height() == -1) {
        if (loadedTable.bottomRight().y() == rowCount - 1)
            q->setContentHeight(loadedTableRect.bottom());
        else
            q->setContentHeight(loadedTableRect.bottom() + flickSpace);
    }
}

void QQuickTableViewPrivate::syncLoadedTableRectFromLoadedTable()
{
    QRectF topLeftRect = loadedTableItem(loadedTable.topLeft())->rect();
    QRectF bottomRightRect = loadedTableItem(loadedTable.bottomRight())->rect();
    loadedTableRect = topLeftRect.united(bottomRightRect);
    loadedTableRectInside = QRectF(topLeftRect.bottomRight(), bottomRightRect.topLeft());
}

void QQuickTableViewPrivate::syncLoadedTableFromLoadRequest()
{
    switch (loadRequest.edgeToLoad) {
    case Qt::LeftEdge:
    case Qt::TopEdge:
        loadedTable.setTopLeft(loadRequest.itemsToLoad.p1());
        break;
    case Qt::RightEdge:
    case Qt::BottomEdge:
        loadedTable.setBottomRight(loadRequest.itemsToLoad.p2());
        break;
    default:
        loadedTable = QRect(loadRequest.itemsToLoad.p1(), loadRequest.itemsToLoad.p2());
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
    // Note: at this point before rebase, sync really means AsyncIfNested
    static const bool forcedAsync = forcedIncubationMode == QLatin1String("async");
    bool async = forcedAsync || incubationMode == QQmlIncubator::Asynchronous;

    QBoolBlocker guard(blockCreatedItemsSyncCallback);

    // Note that even if incubation mode is asynchronous, the item might
    // be ready immediately since the model has a cache of items.
    auto item = static_cast<FxTableItemSG *>(createItem(indexAt(cellCoord), async));
    qCDebug(lcItemViewDelegateLifecycle) << cellCoord << "ready?" << bool(item);
    return item;
}

void QQuickTableViewPrivate::unloadItem(const QPoint &cell)
{
    FxTableItemSG *item = loadedTableItem(cell);
    visibleItems.removeOne(item);
    releaseItem(item);
}

void QQuickTableViewPrivate::unloadItems(const QLine &items)
{
    qCDebug(lcItemViewDelegateLifecycle) << items;
    modified = true;

    if (items.dx()) {
        int y = items.p1().y();
        for (int x = items.p1().x(); x <= items.p2().x(); ++x)
            unloadItem(QPoint(x, y));
    } else {
        int x = items.p1().x();
        for (int y = items.p1().y(); y <= items.p2().y(); ++y)
            unloadItem(QPoint(x, y));
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

QLine QQuickTableViewPrivate::rectangleEdge(const QRect &rect, Qt::Edge tableEdge)
{
    switch (tableEdge) {
    case Qt::LeftEdge:
        return QLine(rect.topLeft(), rect.bottomLeft());
    case Qt::RightEdge:
        return QLine(rect.topRight(), rect.bottomRight());
    case Qt::TopEdge:
        return QLine(rect.topLeft(), rect.topRight());
    case Qt::BottomEdge:
        return QLine(rect.bottomLeft(), rect.bottomRight());
    }
}

QRect QQuickTableViewPrivate::expandedRect(const QRect &rect, Qt::Edge edge, int increment)
{
    switch (edge) {
    case Qt::LeftEdge:
        return rect.adjusted(-increment, 0, 0, 0);
    case Qt::RightEdge:
        return rect.adjusted(0, 0, increment, 0);
    case Qt::TopEdge:
        return rect.adjusted(0, -increment, 0, 0);
    case Qt::BottomEdge:
        return rect.adjusted(0, 0, 0, increment);
    }
}

bool QQuickTableViewPrivate::canLoadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
        if (loadedTable.topLeft().x() == 0)
            return false;
        return loadedTableRect.left() > fillRect.left();
    case Qt::RightEdge:
        if (loadedTable.bottomRight().x() == columnCount - 1)
            return false;
        return loadedTableRect.right() < fillRect.right();
    case Qt::TopEdge:
        if (loadedTable.topLeft().y() == 0)
            return false;
        return loadedTableRect.top() > fillRect.top();
    case Qt::BottomEdge:
        if (loadedTable.bottomRight().y() == rowCount - 1)
            return false;
        return loadedTableRect.bottom() < fillRect.bottom();
    }

    return false;
}

bool QQuickTableViewPrivate::canUnloadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const
{
    // Note: if there is only one row or column left, we cannot unload, since
    // they are needed as anchor point for further layouting.
    const qreal floatingPointMargin = 1;

    switch (tableEdge) {
    case Qt::LeftEdge:
        if (loadedTable.width() <= 1)
            return false;
        return loadedTableRectInside.left() < fillRect.left() - floatingPointMargin;
    case Qt::RightEdge:
        if (loadedTable.width() <= 1)
            return false;
        return loadedTableRectInside.right() > fillRect.right() + floatingPointMargin;
    case Qt::TopEdge:
        if (loadedTable.height() <= 1)
            return false;
        return loadedTableRectInside.top() < fillRect.top() - floatingPointMargin;
    case Qt::BottomEdge:
        if (loadedTable.height() <= 1)
            return false;
        return loadedTableRectInside.bottom() > fillRect.bottom() + floatingPointMargin;
    }
}

qreal QQuickTableViewPrivate::calculateItemX(const FxTableItemSG *fxTableItem, Qt::Edge tableEdge) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
        return itemNextTo(fxTableItem, kRight)->rect().left() - columnSpacing - fxTableItem->rect().width();
    case Qt::RightEdge:
        return itemNextTo(fxTableItem, kLeft)->rect().right() + columnSpacing;
    case Qt::TopEdge:
        return itemNextTo(fxTableItem, kDown)->rect().left();
    case Qt::BottomEdge:
        return itemNextTo(fxTableItem, kUp)->rect().left();
    }

    return 0;
}

qreal QQuickTableViewPrivate::calculateItemY(const FxTableItemSG *fxTableItem, Qt::Edge tableEdge) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
        return itemNextTo(fxTableItem, kRight)->rect().top();
    case Qt::RightEdge:
        return itemNextTo(fxTableItem, kLeft)->rect().top();
    case Qt::TopEdge:
        return itemNextTo(fxTableItem, kDown)->rect().top() - rowSpacing - fxTableItem->rect().height();
    case Qt::BottomEdge:
        return itemNextTo(fxTableItem, kUp)->rect().bottom() + rowSpacing;
    }

    return 0;
}

qreal QQuickTableViewPrivate::calculateItemWidth(const FxTableItemSG *fxTableItem, Qt::Edge tableEdge) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
    case Qt::RightEdge:
        if (coordAt(fxTableItem).y() > loadedTable.y())
            return itemNextTo(fxTableItem, kUp)->rect().width();
        break;
    case Qt::TopEdge:
        return itemNextTo(fxTableItem, kDown)->rect().width();
    case Qt::BottomEdge:
        return itemNextTo(fxTableItem, kUp)->rect().width();
    }

    return fxTableItem->rect().width();
}

qreal QQuickTableViewPrivate::calculateItemHeight(const FxTableItemSG *fxTableItem, Qt::Edge tableEdge) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
        return itemNextTo(fxTableItem, kRight)->rect().height();
    case Qt::RightEdge:
        return itemNextTo(fxTableItem, kLeft)->rect().height();
    case Qt::TopEdge:
    case Qt::BottomEdge:
        if (coordAt(fxTableItem).x() > loadedTable.x())
            return itemNextTo(fxTableItem, kLeft)->rect().height();
        break;
    }

    return fxTableItem->rect().height();
}

void QQuickTableViewPrivate::calculateItemGeometry(FxTableItemSG *fxTableItem, Qt::Edge tableEdge)
{
    qreal w = calculateItemWidth(fxTableItem, tableEdge);
    qreal h = calculateItemHeight(fxTableItem, tableEdge);
    fxTableItem->setSize(QSizeF(w, h), true);

    qreal x = calculateItemX(fxTableItem, tableEdge);
    qreal y = calculateItemY(fxTableItem, tableEdge);
    fxTableItem->setPosition(QPointF(x, y));

    qCDebug(lcItemViewDelegateLifecycle()) << coordAt(fxTableItem) << fxTableItem->rect();
}

void QQuickTableViewPrivate::insertItemIntoTable(FxTableItemSG *fxTableItem)
{
    qCDebug(lcItemViewDelegateLifecycle) << coordAt(fxTableItem);
    modified = true;

    visibleItems.append(fxTableItem);
    calculateItemGeometry(fxTableItem, loadRequest.edgeToLoad);
    showTableItem(fxTableItem);
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

    QPoint loadedCoord = d->coordAt(index);
    qCDebug(lcItemViewDelegateLifecycle) << "item received asynchronously:" << loadedCoord;

    // Check if received an item we don't need. This is expected to
    // happen if we cancelled an ongoing load request.
    if (!d->loadRequest.active) {
        qCDebug(lcItemViewDelegateLifecycle) << "item not needed, load request not active:" << loadedCoord;
        return;
    }

    QPoint waitingCoord = d->coordAt(d->loadRequest.itemsToLoad, d->loadRequest.loadIndex);
    if (loadedCoord != waitingCoord) {
        qCDebug(lcItemViewDelegateLifecycle) << "item not needed, waiting for other item:" << loadedCoord << waitingCoord;
        return;
    }

//    if (loadedCoord == d->loadRequest.itemsToLoad.p2() && d->loadRequest.incubationMode == QQmlIncubator::Asynchronous) {
//        qDebug() << "dont report last item";
//        return;
//    }

    d->processLoadRequest();
    d->loadAndUnloadTableItems();
}

void QQuickTableViewPrivate::cancelLoadRequest()
{
    loadRequest.active = false;

    // Unload already loaded items in the half-completed load
    // request. The currently incubating item, if any, will just
    // be ignored when we receive it.
    int lastLoadedIndex = loadRequest.loadIndex - 1;
    if (lastLoadedIndex < 0)
        return;

    QLine rollbackItems;
    rollbackItems.setP1(loadRequest.itemsToLoad.p1());
    rollbackItems.setP2(coordAt(loadRequest.itemsToLoad, lastLoadedIndex));
    qCDebug(lcItemViewDelegateLifecycle()) << "rollback:" << rollbackItems << tableLayoutToString();
    unloadItems(rollbackItems);
    loadRequest = TableSectionLoadRequest();
}

void QQuickTableViewPrivate::unloadBuffer()
{
    hasBufferedItems = false;
    unloadItemsOutsideRect(viewportRect());
    qCDebug(lcItemViewDelegateLifecycle()) << tableLayoutToString();
}

void QQuickTableViewPrivate::processLoadRequest()
{
    if (!loadRequest.active) {
        loadRequest.active = true;
        if (Qt::Edge edge = loadRequest.edgeToLoad)
            loadRequest.itemsToLoad = rectangleEdge(expandedRect(loadedTable, edge, 1), edge);
        loadRequest.loadCount = lineLength(loadRequest.itemsToLoad);
        qCDebug(lcItemViewDelegateLifecycle()) << "begin:" << loadRequest;
    }

    for (; loadRequest.loadIndex < loadRequest.loadCount; ++loadRequest.loadIndex) {
        QPoint cell = coordAt(loadRequest.itemsToLoad, loadRequest.loadIndex);
        FxTableItemSG *loadedItem = loadTableItem(cell, loadRequest.incubationMode);

        if (!loadedItem) {
            // Requested item is not yet ready. Just leave, and wait for this
            // function to be called again when the item is ready.
            return;
        }

        insertItemIntoTable(loadedItem);
    }

    // Load request done
    syncLoadedTableFromLoadRequest();
    syncLoadedTableRectFromLoadedTable();
    calculateContentSize();
    loadRequest = TableSectionLoadRequest();
    qCDebug(lcItemViewDelegateLifecycle()) << "completed:" << tableLayoutToString();
}

void QQuickTableViewPrivate::loadInitialTopLeftItem()
{
    // Load top-left item. After loaded, loadItemsInsideRect() will take
    // care of filling out the rest of the table.
    QPoint topLeft(0, 0);
    loadRequest.itemsToLoad = QLine(topLeft, topLeft);
    loadRequest.incubationMode = QQmlIncubator::AsynchronousIfNested;
    processLoadRequest();
}

void QQuickTableViewPrivate::unloadItemsOutsideRect(const QRectF &rect)
{
    bool continueUnloadingEdges;

    do {
        continueUnloadingEdges = false;

        for (Qt::Edge edge : allTableEdges) {
            if (canUnloadTableEdge(edge, rect)) {
                unloadItems(rectangleEdge(loadedTable, edge));
                loadedTable = expandedRect(loadedTable, edge, -1);
                syncLoadedTableRectFromLoadedTable();
                continueUnloadingEdges = true;
                qCDebug(lcItemViewDelegateLifecycle) << tableLayoutToString();
            }
        }

    } while (continueUnloadingEdges);
}

void QQuickTableViewPrivate::loadItemsInsideRect(const QRectF &fillRect, QQmlIncubator::IncubationMode incubationMode)
{    
    bool continueLoadingEdges;

    do {
        continueLoadingEdges = false;

        for (Qt::Edge edge : allTableEdges) {
             if (canLoadTableEdge(edge, fillRect)) {
                loadRequest.edgeToLoad = edge;
                loadRequest.incubationMode = incubationMode;
                processLoadRequest();
                continueLoadingEdges = !loadRequest.active;
                break;
            }
        }

    } while (continueLoadingEdges);
}

void QQuickTableViewPrivate::loadAndUnloadTableItems()
{
    if (loadRequest.active)
        return;

    QRectF visibleRect = viewportRect();
    QRectF bufferRect = visibleRect.adjusted(-buffer, -buffer, buffer, buffer);

    if (!visibleRect.isValid())
        return;

    unloadItemsOutsideRect(hasBufferedItems ? bufferRect : visibleRect);
    loadItemsInsideRect(visibleRect, QQmlIncubator::AsynchronousIfNested);

    if (!loadRequest.active && !q_func()->isMoving()) {
        hasBufferedItems = true;
        loadItemsInsideRect(bufferRect, QQmlIncubator::Asynchronous);
    }
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

    QRectF visibleRect = d->viewportRect();

    if (d->loadedTableRect.contains(visibleRect)) {
        // The loaded table still covers the whole viewport. So
        // just check if we should unload any rows/columns, and return.
        if (d->loadRequest.active)
            return;

        if (!d->hasBufferedItems) {
            d->unloadItemsOutsideRect(visibleRect);
        } else {
            QRectF bufferRect = visibleRect.adjusted(-d->buffer, -d->buffer, d->buffer, d->buffer);
            d->unloadItemsOutsideRect(bufferRect);
        }

        return;
    }

    // Since we're at the edge of the loaded table, we need to start loading new
    // rows and columns. To not load more items than what ends up visible, we trim
    // down the table to only cover the viewport before we start loading.
    if (d->hasBufferedItems) {
        d->unloadBuffer();
        d->cancelLoadRequest();
    }

    d->loadAndUnloadTableItems();
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
