/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#define Q_TABLEVIEW_UNREACHABLE(output) { dumpTable(); qDebug() << "output:" << output; Q_UNREACHABLE(); }
#define Q_TABLEVIEW_ASSERT(cond, output) Q_ASSERT(cond || [&](){ dumpTable(); qDebug() << "output:" << output; return false;}())

static const Qt::Edge allTableEdges[] = { Qt::LeftEdge, Qt::RightEdge, Qt::TopEdge, Qt::BottomEdge };

class FxTableItem : public AbstractFxViewItem
{
public:
    FxTableItem(QQuickItem *i, QQuickTableView *v, bool own)
        : AbstractFxViewItem(i, v, own, static_cast<QQuickAbstractItemViewAttached*>(qmlAttachedPropertiesObject<QQuickTableView>(i)))
    {
    }

    void setVisible(bool visible)
    {
        QQuickItemPrivate::get(item)->setCulled(!visible);
    }

    void setGeometry(const QRectF &geometry)
    {
        item->setPosition(geometry.topLeft());
        item->setSize(geometry.size());
    }

    QRectF geometry() const
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
    Q_UNREACHABLE();
    return QString();
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

class QQuickTableViewPrivate : public QQuickItemPrivate
{
    Q_DECLARE_PUBLIC(QQuickTableView)

public:
    QQuickTableViewPrivate();

    // Consider factor out to QQuickAbstractItemViewPrivate
    AbstractFxViewItem *newViewItem(int index, QQuickItem *item);
    AbstractFxViewItem *createItem(int modelIndex, QQmlIncubator::IncubationMode incubationMode);
    bool releaseItem(AbstractFxViewItem *item);
    void releaseLoadedItems();
    void clear();
    bool ownModel;
    // ----------------------------------------------------

    void updatePolish() override;

protected:
    QList<FxTableItem *> visibleItems;

    QVariant modelVariant;
    QPointer<QQmlInstanceModel> model;
    bool delegateValidated;

    QRect loadedTable;
    QRectF loadedTableRect;
    QRectF loadedTableRectInside;
    QRectF loadedTableRectWithUnloadMargins;
    QRectF visibleRect;
    QRectF bufferRect;

    QQmlNullableValue<int> rowCount;
    QQmlNullableValue<int> columnCount;

    qreal rowSpacing;
    qreal columnSpacing;

    TableSectionLoadRequest loadRequest;

    QSizeF accurateContentSize;
    bool contentWidthSetExplicit;
    bool contentHeightSetExplicit;

    int buffer;

    bool modified;
    bool usingBuffer;
    bool blockCreatedItemsSyncCallback;
    bool blockViewportMovedCallback;
    bool layoutInvalid;

    QString forcedIncubationMode;

    constexpr static QPoint kLeft = QPoint(-1, 0);
    constexpr static QPoint kRight = QPoint(1, 0);
    constexpr static QPoint kUp = QPoint(0, -1);
    constexpr static QPoint kDown = QPoint(0, 1);

protected:
    bool isRightToLeft() const;
    bool isBottomToTop() const;

    inline int lineLength(const QLine &line);

    int rowAtIndex(int index) const;
    int columnAtIndex(int index) const;
    int indexAt(const QPoint &cellCoord) const;

    inline QPoint coordAt(int index) const;
    inline QPoint coordAt(const FxTableItem *tableItem) const;
    inline QPoint coordAt(const QLine &line, int index);

    FxTableItem *loadedTableItem(int modelIndex) const;
    inline FxTableItem *loadedTableItem(const QPoint &cellCoord) const;
    inline FxTableItem *itemNextTo(const FxTableItem *fxViewItem, const QPoint &direction) const;
    QLine rectangleEdge(const QRect &rect, Qt::Edge tableEdge);

    QRect expandedRect(const QRect &rect, Qt::Edge edge, int increment);

    qreal calculateItemX(const FxTableItem *fxTableItem, Qt::Edge tableEdge) const;
    qreal calculateItemY(const FxTableItem *fxTableItem, Qt::Edge tableEdge) const;
    qreal calculateItemWidth(const FxTableItem *fxTableItem, Qt::Edge tableEdge) const;
    qreal calculateItemHeight(const FxTableItem *fxTableItem, Qt::Edge tableEdge) const;

    void insertItemIntoTable(FxTableItem *fxTableItem);

    QRectF calculateItemGeometry(FxTableItem *fxTableItem, Qt::Edge tableEdge);
    void calculateContentSize();

    void syncLoadedTableRectFromLoadedTable();
    void syncLoadedTableFromLoadRequest();
    void updateVisibleRectAndBufferRect();

    bool canLoadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const;
    bool canUnloadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const;

    FxTableItem *loadTableItem(const QPoint &cellCoord, QQmlIncubator::IncubationMode incubationMode);

    void unloadItem(const QPoint &cell);
    void unloadItems(const QLine &items);

    void loadInitialTopLeftItem();
    void loadEdgesInsideRect(const QRectF &fillRect, QQmlIncubator::IncubationMode incubationMode);
    void unloadEdgesOutsideRect(const QRectF &rect);
    void loadAndUnloadTableEdges();

    void cancelLoadRequest();
    void processLoadRequest();

    bool isViewportMoving();
    void viewportMoved();
    void invalidateLayout();

    void useWrapperDelegateModel(bool use);

    inline QString tableLayoutToString() const;
    void dumpTable() const;
};

QQuickTableViewPrivate::QQuickTableViewPrivate()
    : QQuickItemPrivate()
    , ownModel(false)
    , delegateValidated(false)
    , loadedTable(QRect())
    , loadedTableRect(QRectF())
    , loadedTableRectInside(QRectF())
    , visibleRect(QRectF())
    , bufferRect(QRectF())
    , rowCount(QQmlNullableValue<int>())
    , columnCount(QQmlNullableValue<int>())
    , rowSpacing(0)
    , columnSpacing(0)
    , loadRequest(TableSectionLoadRequest())
    , accurateContentSize(QSizeF(-1, -1))
    , contentWidthSetExplicit(false)
    , contentHeightSetExplicit(false)
    , buffer(300)
    , modified(false)
    , usingBuffer(false)
    , blockCreatedItemsSyncCallback(false)
    , blockViewportMovedCallback(false)
    , layoutInvalid(true)
    , forcedIncubationMode(qEnvironmentVariable("QT_TABLEVIEW_INCUBATION_MODE"))
{
}

int QQuickTableViewPrivate::lineLength(const QLine &line)
{
    // Note: line needs to be either vertical or horisontal
    return line.x2() - line.x1() + line.y2() - line.y1() + 1;
}

int QQuickTableViewPrivate::rowAtIndex(int index) const
{
    Q_Q(const QQuickTableView);
    int rows = q->rows();
    if (index < 0 || rows <= 0)
        return -1;
    return index % rows;
}

int QQuickTableViewPrivate::columnAtIndex(int index) const
{
    Q_Q(const QQuickTableView);
    int rows = q->rows();
    if (index < 0 || rows <= 0)
        return -1;
    return int(index / rows);
}

QPoint QQuickTableViewPrivate::coordAt(int index) const
{
    return QPoint(columnAtIndex(index), rowAtIndex(index));
}

QPoint QQuickTableViewPrivate::coordAt(const FxTableItem *tableItem) const
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
    return cellCoord.y() + (cellCoord.x() * rows);
}

FxTableItem *QQuickTableViewPrivate::loadedTableItem(int modelIndex) const
{
    for (int i = 0; i < visibleItems.count(); ++i) {
        FxTableItem *item = visibleItems.at(i);
        if (item->index == modelIndex)
            return item;
    }

    return nullptr;
}

FxTableItem *QQuickTableViewPrivate::loadedTableItem(const QPoint &cellCoord) const
{
    return loadedTableItem(indexAt(cellCoord));
}

void QQuickTableViewPrivate::calculateContentSize()
{
    Q_Q(QQuickTableView);

    const qreal flickSpace = 500;

    // Changing content size can sometimes lead to a call to viewportMoved(). But this
    // can cause us to recurse into loading more edges, which we need to block.
    QBoolBlocker guard(blockViewportMovedCallback, true);

    // TODO
//    if (!contentWidthSetExplicit && accurateContentSize.width() == -1) {
//        if (loadedTable.topRight().x() == q->columns() - 1)
//            q->setContentWidth(loadedTableRect.right());
//        else
//            q->setContentWidth(loadedTableRect.right() + flickSpace);
//    }

//    if (!contentHeightSetExplicit && accurateContentSize.height() == -1) {
//        if (loadedTable.bottomRight().y() == q->rows() - 1)
//            q->setContentHeight(loadedTableRect.bottom());
//        else
//            q->setContentHeight(loadedTableRect.bottom() + flickSpace);
//    }
}

void QQuickTableViewPrivate::syncLoadedTableRectFromLoadedTable()
{
    Q_Q(QQuickTableView);

    QRectF topLeftRect = loadedTableItem(loadedTable.topLeft())->geometry();
    QRectF bottomRightRect = loadedTableItem(loadedTable.bottomRight())->geometry();
    loadedTableRect = topLeftRect.united(bottomRightRect);
    loadedTableRectInside = QRectF(topLeftRect.bottomRight(), bottomRightRect.topLeft());

    // We maintain an additional rect that adds some extra margins to loadedTableRect when
    // the loadedTableRect is at the edge of the logical table. This is just convenient when
    // we in viewportMoved() need to check if the table size should be trimmed to speed up
    // edge loading (since we then don't need to add explicit checks for this each time the
    // viewport moves). The margins ensures that we don't unload just because the user
    // overshoots when flicking.
    loadedTableRectWithUnloadMargins = loadedTableRect;
    // TODO
//    if (loadedTableRect.x() == 0)
//        loadedTableRectWithUnloadMargins.setLeft(-q->width());
//    if (loadedTableRect.y() == 0)
//        loadedTableRectWithUnloadMargins.setTop(-q->height());
//    if (loadedTableRect.width() == q->contentWidth())
//        loadedTableRectWithUnloadMargins.setRight(q->contentWidth() + q->width());
//    if (loadedTableRect.y() == q->contentHeight())
//        loadedTableRectWithUnloadMargins.setBottom(q->contentHeight() + q->height());
}

void QQuickTableViewPrivate::updateVisibleRectAndBufferRect()
{
    Q_Q(QQuickTableView);
    visibleRect = QRectF(QPointF(0, 0), q->size());
    // TODO
//    visibleRect = QRectF(QPointF(q->contentX(), q->contentY()), q->size());
    bufferRect = visibleRect.adjusted(-buffer, -buffer, buffer, buffer);
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

AbstractFxViewItem *QQuickTableViewPrivate::newViewItem(int index, QQuickItem *item)
{
    Q_Q(QQuickTableView);
    Q_UNUSED(index);
    return new FxTableItem(item, q, false);
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
    std::stable_sort(listCopy.begin(), listCopy.end(), [](const FxTableItem *lhs, const FxTableItem *rhs) { return lhs->index < rhs->index; });

    for (int i = 0; i < listCopy.count(); ++i) {
        FxTableItem *item = static_cast<FxTableItem *>(listCopy.at(i));
        qDebug() << coordAt(item);
    }

    qDebug() << tableLayoutToString();

    QString filename = QStringLiteral("qquicktableview_dumptable_capture.png");
    if (q_func()->window()->grabWindow().save(filename))
        qDebug() << "Window capture saved to:" << filename;
}

AbstractFxViewItem *QQuickTableViewPrivate::createItem(int modelIndex, QQmlIncubator::IncubationMode incubationMode)
{
    Q_Q(QQuickTableView);

    QObject* object = model->object(modelIndex, incubationMode);

    if (model->incubationStatus(modelIndex) == QQmlIncubator::Loading)
        return nullptr;

    QQuickItem *item = qmlobject_cast<QQuickItem*>(object);
    if (!item) {
        model->release(object);

        if (!delegateValidated) {
            delegateValidated = true;
            QObject* delegate = q->delegate();
            qmlWarning(delegate ? delegate : q) << QQuickTableView::tr("Delegate must be of Item type");
        }

        return nullptr;
    }

    // TODO
//    item->setParentItem(q->contentItem());
    item->setParentItem(q);
    AbstractFxViewItem *viewItem = newViewItem(modelIndex, item);

    if (viewItem) {
        viewItem->index = modelIndex;
        // do other set up for the new item that should not happen
        // until after bindings are evaluated
        //            initializeViewItem(viewItem);
    }

    return viewItem;
}

void QQuickTableViewPrivate::releaseLoadedItems() {
    // Make a copy and clear the visibleItems first to avoid destroyed
    // items being accessed during the loop (QTBUG-61294)
    const QList<FxTableItem *> oldVisible = visibleItems;
    visibleItems.clear();
    for (FxTableItem *item : oldVisible)
        releaseItem(item);
}

bool QQuickTableViewPrivate::releaseItem(AbstractFxViewItem *item)
{
    if (item->item) {
        if (model->release(item->item) == QQmlInstanceModel::Destroyed)
            item->item->setParentItem(0);
    }

    delete item;
    return flags != QQmlInstanceModel::Referenced;
}

void QQuickTableViewPrivate::clear()
{
    releaseLoadedItems();

    if (loadRequest.active) {
        model->cancel(loadRequest.loadIndex);
        loadRequest = TableSectionLoadRequest();
    }
}

FxTableItem *QQuickTableViewPrivate::loadTableItem(const QPoint &cellCoord, QQmlIncubator::IncubationMode incubationMode)
{
#ifdef QT_DEBUG
    // Since TableView needs to work flawlessly when e.g incubating inside an async
    // loader, being able to override all loading to async while debugging can be helpful.
    static const bool forcedAsync = forcedIncubationMode == QLatin1String("async");
    if (forcedAsync)
        incubationMode = QQmlIncubator::Asynchronous;
#endif

    // Note that even if incubation mode is asynchronous, the item might
    // be ready immediately since the model has a cache of items.
    QBoolBlocker guard(blockCreatedItemsSyncCallback);
    auto item = static_cast<FxTableItem *>(createItem(indexAt(cellCoord), incubationMode));
    qCDebug(lcItemViewDelegateLifecycle) << cellCoord << "ready?" << bool(item);
    return item;
}

void QQuickTableViewPrivate::unloadItem(const QPoint &cell)
{
    FxTableItem *item = loadedTableItem(cell);
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

FxTableItem *QQuickTableViewPrivate::itemNextTo(const FxTableItem *fxViewItem, const QPoint &direction) const
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
    Q_TABLEVIEW_UNREACHABLE(tableEdge);
    return QLine();
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
    Q_TABLEVIEW_UNREACHABLE(edge);
    return QRect();
}

bool QQuickTableViewPrivate::canLoadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const
{
    Q_Q(const QQuickTableView);

    switch (tableEdge) {
    case Qt::LeftEdge:
        if (loadedTable.topLeft().x() == 0)
            return false;
        return loadedTableRect.left() > fillRect.left();
    case Qt::RightEdge:
        if (loadedTable.bottomRight().x() >= q->columns() - 1)
            return false;
        return loadedTableRect.right() < fillRect.right();
    case Qt::TopEdge:
        if (loadedTable.topLeft().y() == 0)
            return false;
        return loadedTableRect.top() > fillRect.top();
    case Qt::BottomEdge:
        if (loadedTable.bottomRight().y() >= q->rows() - 1)
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
    Q_TABLEVIEW_UNREACHABLE(tableEdge);
    return false;
}

qreal QQuickTableViewPrivate::calculateItemX(const FxTableItem *fxTableItem, Qt::Edge tableEdge) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
        return itemNextTo(fxTableItem, kRight)->geometry().left() - columnSpacing - fxTableItem->geometry().width();
    case Qt::RightEdge:
        return itemNextTo(fxTableItem, kLeft)->geometry().right() + columnSpacing;
    case Qt::TopEdge:
        return itemNextTo(fxTableItem, kDown)->geometry().left();
    case Qt::BottomEdge:
        return itemNextTo(fxTableItem, kUp)->geometry().left();
    }

    return 0;
}

qreal QQuickTableViewPrivate::calculateItemY(const FxTableItem *fxTableItem, Qt::Edge tableEdge) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
        return itemNextTo(fxTableItem, kRight)->geometry().top();
    case Qt::RightEdge:
        return itemNextTo(fxTableItem, kLeft)->geometry().top();
    case Qt::TopEdge:
        return itemNextTo(fxTableItem, kDown)->geometry().top() - rowSpacing - fxTableItem->geometry().height();
    case Qt::BottomEdge:
        return itemNextTo(fxTableItem, kUp)->geometry().bottom() + rowSpacing;
    }

    return 0;
}

qreal QQuickTableViewPrivate::calculateItemWidth(const FxTableItem *fxTableItem, Qt::Edge tableEdge) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
    case Qt::RightEdge:
        if (coordAt(fxTableItem).y() > loadedTable.y())
            return itemNextTo(fxTableItem, kUp)->geometry().width();
        break;
    case Qt::TopEdge:
        return itemNextTo(fxTableItem, kDown)->geometry().width();
    case Qt::BottomEdge:
        return itemNextTo(fxTableItem, kUp)->geometry().width();
    }

    return fxTableItem->geometry().width();
}

qreal QQuickTableViewPrivate::calculateItemHeight(const FxTableItem *fxTableItem, Qt::Edge tableEdge) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
        return itemNextTo(fxTableItem, kRight)->geometry().height();
    case Qt::RightEdge:
        return itemNextTo(fxTableItem, kLeft)->geometry().height();
    case Qt::TopEdge:
    case Qt::BottomEdge:
        if (coordAt(fxTableItem).x() > loadedTable.x())
            return itemNextTo(fxTableItem, kLeft)->geometry().height();
        break;
    }

    return fxTableItem->geometry().height();
}

QRectF QQuickTableViewPrivate::calculateItemGeometry(FxTableItem *fxTableItem, Qt::Edge tableEdge)
{
    qreal x = calculateItemX(fxTableItem, tableEdge);
    qreal y = calculateItemY(fxTableItem, tableEdge);
    qreal w = calculateItemWidth(fxTableItem, tableEdge);
    qreal h = calculateItemHeight(fxTableItem, tableEdge);
    return QRectF(x, y, w, h);
}

void QQuickTableViewPrivate::insertItemIntoTable(FxTableItem *fxTableItem)
{
    modified = true;

    visibleItems.append(fxTableItem);
    fxTableItem->setGeometry(calculateItemGeometry(fxTableItem, loadRequest.edgeToLoad));
    fxTableItem->setVisible(true);

    qCDebug(lcItemViewDelegateLifecycle) << coordAt(fxTableItem) << "geometry:" << fxTableItem->geometry();
}

void QQuickTableView::createdItem(int index, QObject*)
{
    Q_D(QQuickTableView);

    if (d->blockCreatedItemsSyncCallback)
        return;

    QPoint loadedCoord = d->coordAt(index);
    qCDebug(lcItemViewDelegateLifecycle) << "item received asynchronously:" << loadedCoord;

    d->processLoadRequest();
    d->loadAndUnloadTableEdges();
}

void QQuickTableViewPrivate::cancelLoadRequest()
{
    loadRequest.active = false;

    model->cancel(loadRequest.loadIndex);

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
        FxTableItem *loadedItem = loadTableItem(cell, loadRequest.incubationMode);

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

void QQuickTableViewPrivate::unloadEdgesOutsideRect(const QRectF &rect)
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

void QQuickTableViewPrivate::loadEdgesInsideRect(const QRectF &fillRect, QQmlIncubator::IncubationMode incubationMode)
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

void QQuickTableViewPrivate::loadAndUnloadTableEdges()
{
    // Load and unload the edges around the current table until we cover the
    // whole viewport, and then load more items async until we filled up the
    // requested buffer around the viewport as well.
    // Note: an important point is that we always keep the table rectangular
    // and without holes to reduce complexity (we never leave the table in
    // a half-loaded state, or keep track of multiple patches).
    // We load only one edge (row or column) at a time. This is especially
    // important when loading into the buffer, since we need to be able to
    // cancel the buffering quickly if the user starts to flick, and then
    // focus all further loading on the edges that are flicked into view.
    Q_TABLEVIEW_ASSERT(visibleItems.count() > 0, "");
    if (loadRequest.active)
        return;

    unloadEdgesOutsideRect(usingBuffer ? bufferRect : visibleRect);
    loadEdgesInsideRect(visibleRect, QQmlIncubator::AsynchronousIfNested);

    if (!buffer || loadRequest.active || isViewportMoving())
        return;

    // Start buffer table items async outside the viewport
    loadEdgesInsideRect(bufferRect, QQmlIncubator::Asynchronous);
    usingBuffer = true;
}

bool QQuickTableViewPrivate::isViewportMoving()
{
    // TODO:
    return false;
}

void QQuickTableViewPrivate::viewportMoved()
{
    if (blockViewportMovedCallback)
        return;

    updateVisibleRectAndBufferRect();

    if (usingBuffer && !loadedTableRectWithUnloadMargins.contains(visibleRect)) {
        // The visible rect has passed the loaded table rect, including buffered items. This
        // means that we need to load new edges immediately to avoid flicking in empty areas.
        // Since we always keep the table rectangular, we trim the table down to be the size of
        // the visible rect so that each edge contains fewer items. This will make it faster to
        // load new edges as the viewport moves.
        qCDebug(lcItemViewDelegateLifecycle()) << "unload buffer";
        usingBuffer = false;
        unloadEdgesOutsideRect(visibleRect);
        cancelLoadRequest();
    }

    loadAndUnloadTableEdges();
}

void QQuickTableViewPrivate::invalidateLayout() {
    layoutInvalid = true;
    q_func()->polish();
}

void QQuickTableViewPrivate::updatePolish()
{
    if (layoutInvalid) {
        layoutInvalid = false;
        // TODO: dont release all items when
        // e.g just the spacing has changed
        clear();
        loadInitialTopLeftItem();
    }

    updateVisibleRectAndBufferRect();
    loadAndUnloadTableEdges();
}

void QQuickTableViewPrivate::useWrapperDelegateModel(bool use)
{
    Q_Q(QQuickTableView);

    if (use == ownModel)
        return;

    if (use) {
        model = new QQmlDelegateModel(qmlContext(q), q);
        if (q->isComponentComplete())
            static_cast<QQmlDelegateModel *>(model.data())->componentComplete();
    } else {
        delete model;
        model = nullptr;
    }

    ownModel = use;
}

QQuickTableView::QQuickTableView(QQuickItem *parent)
    : QQuickItem(*(new QQuickTableViewPrivate), parent)
{
    Q_D(QQuickTableView);

    // TODO
//    connect(this, &QQuickTableView::movingChanged, [=] {
//        d->loadAndUnloadTableEdges();
//    });
    // and connect to viewportMoved
}

int QQuickTableView::rows() const
{
    Q_D(const QQuickTableView);
    if (QQmlDelegateModel *delegateModel = qobject_cast<QQmlDelegateModel *>(d->model.data()))
        return delegateModel->rows();
    return d->rowCount;
}

void QQuickTableView::setRows(int rows)
{
    Q_D(QQuickTableView);
    if (d->rowCount == rows)
        return;

    d->rowCount = rows;
    if (d->componentComplete && d->model && d->ownModel)
        static_cast<QQmlDelegateModel *>(d->model.data())->setRows(rows);

    d->invalidateLayout();
    emit rowsChanged();
}

void QQuickTableView::resetRows()
{
    Q_D(QQuickTableView);
    if (d->rowCount.isNull)
        return;

    d->rowCount = QQmlNullableValue<int>();
    if (d->componentComplete && d->model && d->ownModel)
        static_cast<QQmlDelegateModel *>(d->model.data())->resetRows();

    d->invalidateLayout();
    emit rowsChanged();
}

int QQuickTableView::columns() const
{
    Q_D(const QQuickTableView);
    if (QQmlDelegateModel *delegateModel = qobject_cast<QQmlDelegateModel *>(d->model.data()))
        return delegateModel->columns();
    return d->columnCount;
}

void QQuickTableView::setColumns(int columns)
{
    Q_D(QQuickTableView);
    if (d->columnCount == columns)
        return;

    d->columnCount = columns;
    if (d->componentComplete && d->model && d->ownModel)
        static_cast<QQmlDelegateModel *>(d->model.data())->setColumns(columns);

    d->invalidateLayout();
    emit columnsChanged();
}

void QQuickTableView::resetColumns()
{
    Q_D(QQuickTableView);
    if (d->columnCount.isNull)
        return;

    d->columnCount = QQmlNullableValue<int>();
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
    d->invalidateLayout();
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
    d->invalidateLayout();
    emit columnSpacingChanged();
}

int QQuickTableView::cacheBuffer() const
{
    return d_func()->buffer;
}

void QQuickTableView::setCacheBuffer(int newBuffer)
{
    Q_D(QQuickTableView);
    if (d->buffer == newBuffer)
        return;

    if (newBuffer < 0) {
        qmlWarning(this) << "Cannot set a negative cache buffer";
        return;
    }

    d->buffer = newBuffer;
    d->invalidateLayout();
    emit cacheBufferChanged();
}

QVariant QQuickTableView::model() const
{
    return d_func()->modelVariant;
}

void QQuickTableView::setModel(const QVariant &newModel)
{
    Q_D(QQuickTableView);
    QVariant model = newModel;

    if (model.userType() == qMetaTypeId<QJSValue>())
        model = model.value<QJSValue>().toVariant();

    if (d->modelVariant == model)
        return;

    if (d->model) {
        disconnect(d->model, SIGNAL(initItem(int,QObject*)), this, SLOT(initItem(int,QObject*)));
        disconnect(d->model, SIGNAL(createdItem(int,QObject*)), this, SLOT(createdItem(int,QObject*)));
    }

    d->clear();
    d->modelVariant = model;
    QObject *object = qvariant_cast<QObject*>(model);

    if (auto instanceModel = qobject_cast<QQmlInstanceModel *>(object)) {
        d->useWrapperDelegateModel(false);
        d->model = instanceModel;
    } else {
        d->useWrapperDelegateModel(true);
        qobject_cast<QQmlDelegateModel*>(d->model)->setModel(model);
    }

    if (d->model) {
        connect(d->model, SIGNAL(createdItem(int,QObject*)), this, SLOT(createdItem(int,QObject*)));
        connect(d->model, SIGNAL(initItem(int,QObject*)), this, SLOT(initItem(int,QObject*)));
    }

    d->invalidateLayout();

    emit modelChanged();
}

QQmlComponent *QQuickTableView::delegate() const
{
    if (QQmlDelegateModel *delegateModel = qobject_cast<QQmlDelegateModel*>(d_func()->model))
        return delegateModel->delegate();

    return nullptr;
}

void QQuickTableView::setDelegate(QQmlComponent *delegate)
{
    Q_D(QQuickTableView);
    if (delegate == this->delegate())
        return;

    d->useWrapperDelegateModel(true);
    qobject_cast<QQmlDelegateModel*>(d->model)->setDelegate(delegate);
    d->delegateValidated = false;
    d->invalidateLayout();

    emit delegateChanged();
}

QQuickTableViewAttached *QQuickTableView::qmlAttachedProperties(QObject *obj)
{
    return new QQuickTableViewAttached(obj);
}

void QQuickTableView::initItem(int index, QObject *object)
{
    Q_UNUSED(index);
    // Setting the view from the FxViewItem wrapper is too late if the delegate
    // needs access to the view in Component.onCompleted
    QQuickItem *item = qmlobject_cast<QQuickItem *>(object);
    if (!item)
        return;

    // TODO
//    QObject *attachedObject = qmlAttachedPropertiesObject<QQuickTableView>(item);
//    if (QQuickTableViewAttached *attached = static_cast<QQuickTableViewAttached *>(attachedObject))
//        attached->setView(this);
}

void QQuickTableView::componentComplete()
{
    Q_D(QQuickTableView);
    if (d->model && d->ownModel) {
        QQmlDelegateModel *delegateModel = static_cast<QQmlDelegateModel *>(d->model.data());
        if (!d->rowCount.isNull)
            delegateModel->setRows(d->rowCount);
        if (!d->columnCount.isNull)
            delegateModel->setColumns(d->columnCount);
        static_cast<QQmlDelegateModel *>(d->model.data())->componentComplete();
    }

    // TODO
    // Allow app to set content size explicitly, instead of us trying to guess as we go
//    d->contentWidthSetExplicit = (contentWidth() != -1);
//    d->contentHeightSetExplicit = (contentHeight() != -1);

    QQuickItem::componentComplete();
}

QT_END_NAMESPACE
