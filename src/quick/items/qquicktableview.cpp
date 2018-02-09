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

#include <QtQml/private/qqmldelegatemodel_p.h>
#include <QtQml/private/qqmlincubator_p.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcTableViewDelegateLifecycle, "qt.quick.tableview.lifecycle")

#define Q_TABLEVIEW_UNREACHABLE(output) { dumpTable(); qDebug() << "output:" << output; Q_UNREACHABLE(); }
#define Q_TABLEVIEW_ASSERT(cond, output) Q_ASSERT(cond || [&](){ dumpTable(); qDebug() << "output:" << output; return false;}())

static const Qt::Edge allTableEdges[] = { Qt::LeftEdge, Qt::RightEdge, Qt::TopEdge, Qt::BottomEdge };

class FxTableViewItem;

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

class QQuickTableViewPrivate : public QQuickItemPrivate, public QQuickItemChangeListener
{
    Q_DECLARE_PUBLIC(QQuickTableView)

public:
    QQuickTableViewPrivate();

    static inline QQuickTableViewPrivate *get(QQuickTableView *q) { return q->d_func(); }

    void updatePolish() override;

protected:
    QList<FxTableViewItem *> visibleItems;

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

    TableSectionLoadRequest loadRequest;

    QPointF contentPos;
    QSizeF accumulatedTableSize;
    QSizeF spacing;

    int buffer;

    bool modified;
    bool usingBuffer;
    bool blockCreatedItemsSyncCallback;
    bool blockViewportMovedCallback;
    bool layoutInvalid;
    bool ownModel;
    bool interactive;

    QQmlNullableValue<QQuickItem *> contentItem;
    QQuickFlickable *flickable;

#ifdef QT_DEBUG
    QString forcedIncubationMode;
#endif

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
    inline QPoint coordAt(const FxTableViewItem *tableItem) const;
    inline QPoint coordAt(const QLine &line, int index);

    FxTableViewItem *loadedTableItem(int modelIndex) const;
    inline FxTableViewItem *loadedTableItem(const QPoint &cellCoord) const;
    inline FxTableViewItem *itemNextTo(const FxTableViewItem *FxTableViewItem, const QPoint &direction) const;
    QLine rectangleEdge(const QRect &rect, Qt::Edge tableEdge);

    QRect expandedRect(const QRect &rect, Qt::Edge edge, int increment);

    qreal calculateItemX(const FxTableViewItem *FxTableViewItem, Qt::Edge tableEdge) const;
    qreal calculateItemY(const FxTableViewItem *FxTableViewItem, Qt::Edge tableEdge) const;
    qreal calculateItemWidth(const FxTableViewItem *FxTableViewItem, Qt::Edge tableEdge) const;
    qreal calculateItemHeight(const FxTableViewItem *FxTableViewItem, Qt::Edge tableEdge) const;

    void insertItemIntoTable(FxTableViewItem *FxTableViewItem);

    QRectF calculateItemGeometry(FxTableViewItem *FxTableViewItem, Qt::Edge tableEdge);
    void updateTableSize();

    void syncLoadedTableRectFromLoadedTable();
    void syncLoadedTableFromLoadRequest();
    void updateVisibleRectAndBufferRect();

    bool canLoadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const;
    bool canUnloadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const;

    FxTableViewItem *createItem(int modelIndex, QQmlIncubator::IncubationMode incubationMode);
    FxTableViewItem *loadTableItem(const QPoint &cellCoord, QQmlIncubator::IncubationMode incubationMode);
    bool releaseItem(FxTableViewItem *item);
    void releaseLoadedItems();
    void clear();

    void unloadItem(const QPoint &cell);
    void unloadItems(const QLine &items);

    void loadInitialTopLeftItem();
    void loadEdgesInsideRect(const QRectF &fillRect, QQmlIncubator::IncubationMode incubationMode);
    void unloadEdgesOutsideRect(const QRectF &rect);
    void loadAndUnloadTableEdges();

    void cancelLoadRequest();
    void processLoadRequest();

    bool isViewportMoving();
    void viewportChanged();
    void invalidateLayout();

    void useWrapperDelegateModel(bool use);

    void createFlickable();

    inline QString tableLayoutToString() const;
    void dumpTable() const;
};

class FxTableViewItem : public FxViewItemBase
{
public:
    FxTableViewItem(QQuickItem *item, QQuickTableView *view, bool own)
        : FxViewItemBase(item, own, QQuickTableViewPrivate::get(view))
        , view(view)
        , attached(static_cast<QQuickTableViewAttached*>(qmlAttachedPropertiesObject<QQuickTableView>(item)))
    {
        if (attached)
            attached->setView(view);
    }

    qreal position() const { return 0; }
    qreal endPosition() const { return 0; }
    qreal size() const { return 0; }
    qreal sectionSize() const { return 0; }
    bool contains(qreal, qreal) const { return false; }

    QQuickTableView *view;
    QQuickTableViewAttached *attached;
};

QQuickTableViewPrivate::QQuickTableViewPrivate()
    : QQuickItemPrivate()
    , delegateValidated(false)
    , loadedTable(QRect())
    , loadedTableRect(QRectF())
    , loadedTableRectInside(QRectF())
    , visibleRect(QRectF())
    , bufferRect(QRectF())
    , rowCount(QQmlNullableValue<int>())
    , columnCount(QQmlNullableValue<int>())
    , loadRequest(TableSectionLoadRequest())
    , contentPos(QPointF())
    , accumulatedTableSize(QSizeF())
    , spacing(QSize())
    , buffer(300)
    , modified(false)
    , usingBuffer(false)
    , blockCreatedItemsSyncCallback(false)
    , blockViewportMovedCallback(false)
    , layoutInvalid(true)
    , ownModel(false)
    , interactive(true)
    , flickable(nullptr)
#ifdef QT_DEBUG
    , forcedIncubationMode(qEnvironmentVariable("QT_TABLEVIEW_INCUBATION_MODE"))
#endif
{
}

int QQuickTableViewPrivate::lineLength(const QLine &line)
{
    // Note: line needs to be either vertical or horizontal
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

QPoint QQuickTableViewPrivate::coordAt(const FxTableViewItem *tableItem) const
{
    return coordAt(tableItem->index);
}

QPoint QQuickTableViewPrivate::coordAt(const QLine &line, int index)
{
    // Note: a line is either vertical or horizontal
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

FxTableViewItem *QQuickTableViewPrivate::loadedTableItem(int modelIndex) const
{
    for (int i = 0; i < visibleItems.count(); ++i) {
        FxTableViewItem *item = visibleItems.at(i);
        if (item->index == modelIndex)
            return item;
    }

    return nullptr;
}

FxTableViewItem *QQuickTableViewPrivate::loadedTableItem(const QPoint &cellCoord) const
{
    return loadedTableItem(indexAt(cellCoord));
}

void QQuickTableViewPrivate::updateTableSize()
{
    Q_Q(QQuickTableView);

    bool atTableEndX = loadedTable.topRight().x() == q->columns() - 1;
    bool atTableEndY = loadedTable.bottomRight().y() == q->rows() - 1;

    const qreal flickSpace = 500;
    qreal newWidth = loadedTableRect.right() + (atTableEndX ? 0 : flickSpace);
    qreal newHeight = loadedTableRect.bottom() + (atTableEndY ? 0 : flickSpace);

    // Changing content size can sometimes lead to a call to viewportMoved(). But this
    // can cause us to recurse into loading more edges, which we need to block.
    QBoolBlocker guard(blockViewportMovedCallback, true);

    if (newWidth > accumulatedTableSize.width() || atTableEndX) {
        accumulatedTableSize.setWidth(newWidth);
        emit q->accumulatedTableWidthChanged();
    }

    if (newHeight > accumulatedTableSize.height() || atTableEndY) {
        accumulatedTableSize.setHeight(newHeight);
        emit q->accumulatedTableHeightChanged();
    }
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

    if (loadedTableRect.x() == 0)
        loadedTableRectWithUnloadMargins.setLeft(-q->width());
    if (loadedTableRect.y() == 0)
        loadedTableRectWithUnloadMargins.setTop(-q->height());
    if (loadedTableRect.width() == accumulatedTableSize.width())
        loadedTableRectWithUnloadMargins.setRight(accumulatedTableSize.width() + q->width());
    if (loadedTableRect.y() == accumulatedTableSize.height())
        loadedTableRectWithUnloadMargins.setBottom(accumulatedTableSize.height() + q->height());
}

void QQuickTableViewPrivate::updateVisibleRectAndBufferRect()
{
    Q_Q(QQuickTableView);
    visibleRect = QRectF(contentPos, q->size());
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
    std::stable_sort(listCopy.begin(), listCopy.end(), [](const FxTableViewItem *lhs, const FxTableViewItem *rhs) { return lhs->index < rhs->index; });

    for (int i = 0; i < listCopy.count(); ++i) {
        FxTableViewItem *item = static_cast<FxTableViewItem *>(listCopy.at(i));
        qDebug() << coordAt(item);
    }

    qDebug() << tableLayoutToString();

    QString filename = QStringLiteral("qquicktableview_dumptable_capture.png");
    if (q_func()->window()->grabWindow().save(filename))
        qDebug() << "Window capture saved to:" << filename;
}

FxTableViewItem *QQuickTableViewPrivate::createItem(int modelIndex, QQmlIncubator::IncubationMode incubationMode)
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

    item->setParentItem(contentItem);

    FxTableViewItem *viewItem = new FxTableViewItem(item, q, false);

    if (viewItem)
        viewItem->index = modelIndex;

    return viewItem;
}

void QQuickTableViewPrivate::releaseLoadedItems() {
    // Make a copy and clear the visibleItems first to avoid destroyed
    // items being accessed during the loop (QTBUG-61294)
    const QList<FxTableViewItem *> oldVisible = visibleItems;
    visibleItems.clear();
    for (FxTableViewItem *item : oldVisible)
        releaseItem(item);
}

bool QQuickTableViewPrivate::releaseItem(FxTableViewItem *item)
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

FxTableViewItem *QQuickTableViewPrivate::loadTableItem(const QPoint &cellCoord, QQmlIncubator::IncubationMode incubationMode)
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
    auto item = static_cast<FxTableViewItem *>(createItem(indexAt(cellCoord), incubationMode));
    qCDebug(lcTableViewDelegateLifecycle) << cellCoord << "ready?" << bool(item);
    return item;
}

void QQuickTableViewPrivate::unloadItem(const QPoint &cell)
{
    FxTableViewItem *item = loadedTableItem(cell);
    visibleItems.removeOne(item);
    releaseItem(item);
}

void QQuickTableViewPrivate::unloadItems(const QLine &items)
{
    qCDebug(lcTableViewDelegateLifecycle) << items;
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

FxTableViewItem *QQuickTableViewPrivate::itemNextTo(const FxTableViewItem *FxTableViewItem, const QPoint &direction) const
{
    return loadedTableItem(coordAt(FxTableViewItem) + direction);
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

qreal QQuickTableViewPrivate::calculateItemX(const FxTableViewItem *FxTableViewItem, Qt::Edge tableEdge) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
        return itemNextTo(FxTableViewItem, kRight)->geometry().left() - spacing.width() - FxTableViewItem->geometry().width();
    case Qt::RightEdge:
        return itemNextTo(FxTableViewItem, kLeft)->geometry().right() + spacing.width();
    case Qt::TopEdge:
        return itemNextTo(FxTableViewItem, kDown)->geometry().left();
    case Qt::BottomEdge:
        return itemNextTo(FxTableViewItem, kUp)->geometry().left();
    }

    return 0;
}

qreal QQuickTableViewPrivate::calculateItemY(const FxTableViewItem *FxTableViewItem, Qt::Edge tableEdge) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
        return itemNextTo(FxTableViewItem, kRight)->geometry().top();
    case Qt::RightEdge:
        return itemNextTo(FxTableViewItem, kLeft)->geometry().top();
    case Qt::TopEdge:
        return itemNextTo(FxTableViewItem, kDown)->geometry().top() - spacing.height() - FxTableViewItem->geometry().height();
    case Qt::BottomEdge:
        return itemNextTo(FxTableViewItem, kUp)->geometry().bottom() + spacing.height();
    }

    return 0;
}

qreal QQuickTableViewPrivate::calculateItemWidth(const FxTableViewItem *FxTableViewItem, Qt::Edge tableEdge) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
    case Qt::RightEdge:
        if (coordAt(FxTableViewItem).y() > loadedTable.y())
            return itemNextTo(FxTableViewItem, kUp)->geometry().width();
        break;
    case Qt::TopEdge:
        return itemNextTo(FxTableViewItem, kDown)->geometry().width();
    case Qt::BottomEdge:
        return itemNextTo(FxTableViewItem, kUp)->geometry().width();
    }

    return FxTableViewItem->geometry().width();
}

qreal QQuickTableViewPrivate::calculateItemHeight(const FxTableViewItem *FxTableViewItem, Qt::Edge tableEdge) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
        return itemNextTo(FxTableViewItem, kRight)->geometry().height();
    case Qt::RightEdge:
        return itemNextTo(FxTableViewItem, kLeft)->geometry().height();
    case Qt::TopEdge:
    case Qt::BottomEdge:
        if (coordAt(FxTableViewItem).x() > loadedTable.x())
            return itemNextTo(FxTableViewItem, kLeft)->geometry().height();
        break;
    }

    return FxTableViewItem->geometry().height();
}

QRectF QQuickTableViewPrivate::calculateItemGeometry(FxTableViewItem *FxTableViewItem, Qt::Edge tableEdge)
{
    qreal x = calculateItemX(FxTableViewItem, tableEdge);
    qreal y = calculateItemY(FxTableViewItem, tableEdge);
    qreal w = calculateItemWidth(FxTableViewItem, tableEdge);
    qreal h = calculateItemHeight(FxTableViewItem, tableEdge);
    return QRectF(x, y, w, h);
}

void QQuickTableViewPrivate::insertItemIntoTable(FxTableViewItem *FxTableViewItem)
{
    modified = true;

    visibleItems.append(FxTableViewItem);
    FxTableViewItem->setGeometry(calculateItemGeometry(FxTableViewItem, loadRequest.edgeToLoad));
    FxTableViewItem->setVisible(true);

    qCDebug(lcTableViewDelegateLifecycle) << coordAt(FxTableViewItem) << "geometry:" << FxTableViewItem->geometry();
}

void QQuickTableView::createdItem(int index, QObject*)
{
    Q_D(QQuickTableView);

    if (d->blockCreatedItemsSyncCallback)
        return;

    QPoint loadedCoord = d->coordAt(index);
    qCDebug(lcTableViewDelegateLifecycle) << "item received asynchronously:" << loadedCoord;

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
    qCDebug(lcTableViewDelegateLifecycle()) << "rollback:" << rollbackItems << tableLayoutToString();
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
        qCDebug(lcTableViewDelegateLifecycle()) << "begin:" << loadRequest;
    }

    for (; loadRequest.loadIndex < loadRequest.loadCount; ++loadRequest.loadIndex) {
        QPoint cell = coordAt(loadRequest.itemsToLoad, loadRequest.loadIndex);
        FxTableViewItem *loadedItem = loadTableItem(cell, loadRequest.incubationMode);

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
    updateTableSize();
    loadRequest = TableSectionLoadRequest();
    qCDebug(lcTableViewDelegateLifecycle()) << "completed:" << tableLayoutToString();
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
                qCDebug(lcTableViewDelegateLifecycle) << tableLayoutToString();
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
    return flickable ? flickable->isMoving() : false;
}

void QQuickTableViewPrivate::viewportChanged()
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
        qCDebug(lcTableViewDelegateLifecycle()) << "unload buffer";
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

void QQuickTableViewPrivate::createFlickable()
{
    Q_Q(QQuickTableView);
    Q_TABLEVIEW_ASSERT(!flickable, "");

    flickable = new QQuickFlickable(q);
    if (contentItem.isNull)
        contentItem = flickable->contentItem();
    flickable->setWidth(q->width());
    flickable->setHeight(q->height());

    q->connect(flickable, &QQuickFlickable::movingChanged, [=]{
        viewportChanged();
    });

    q->connect(flickable, &QQuickFlickable::contentXChanged, [=]{
        q->setContentX(flickable->contentX());
    });

    q->connect(flickable, &QQuickFlickable::contentYChanged, [=]{
        q->setContentY(flickable->contentY());
    });

    q->connect(q, &QQuickTableView::accumulatedTableWidthChanged, [=]{
        flickable->setContentWidth(q->accumulatedTableWidth());
    });

    q->connect(q, &QQuickTableView::accumulatedTableHeightChanged, [=]{
        flickable->setContentHeight(q->accumulatedTableHeight());
    });

    q->connect(q, &QQuickTableView::widthChanged, [=]{
        flickable->setWidth(q->width());
    });

    q->connect(q, &QQuickTableView::heightChanged, [=]{
        flickable->setHeight(q->height());
    });
}

QQuickTableView::QQuickTableView(QQuickItem *parent)
    : QQuickItem(*(new QQuickTableViewPrivate), parent)
{
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
    return d_func()->spacing.height();
}

void QQuickTableView::setRowSpacing(qreal spacing)
{
    Q_D(QQuickTableView);
    if (qFuzzyCompare(d->spacing.height(), spacing))
        return;

    d->spacing.setHeight(spacing);
    d->invalidateLayout();
    emit rowSpacingChanged();
}

qreal QQuickTableView::columnSpacing() const
{
    return d_func()->spacing.width();
}

void QQuickTableView::setColumnSpacing(qreal spacing)
{
    Q_D(QQuickTableView);
    if (qFuzzyCompare(d->spacing.width(), spacing))
        return;

    d->spacing.setWidth(spacing);
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

qreal QQuickTableView::accumulatedTableWidth() const
{
    return d_func()->accumulatedTableSize.width();
}

qreal QQuickTableView::accumulatedTableHeight() const
{
    return d_func()->accumulatedTableSize.height();
}

bool QQuickTableView::interactive() const
{
    return d_func()->interactive;
}

void QQuickTableView::setInteractive(bool interactive)
{
    Q_D(QQuickTableView);
    if (interactive == d->interactive)
        return;

    d->interactive = interactive;

    if (interactive) {
        if (!d->flickable)
            d->createFlickable();
    } else {
        delete d->flickable;
        d->flickable = nullptr;
    }

    emit interactiveChanged();
}

QQuickTableViewAttached *QQuickTableView::qmlAttachedProperties(QObject *obj)
{
    return new QQuickTableViewAttached(obj);
}

QQuickItem *QQuickTableView::contentItem() const
{
    return d_func()->contentItem;
}

void QQuickTableView::setContentItem(QQuickItem *contentItem)
{
    Q_D(QQuickTableView);
    if (d->contentItem == contentItem)
        return;

    d->contentItem = contentItem;
    d->invalidateLayout();
    emit contentItemChanged();
}

qreal QQuickTableView::contentX() const
{
    return d_func()->contentPos.x();
}

qreal QQuickTableView::contentY() const
{
    return d_func()->contentPos.y();
}

void QQuickTableView::setContentX(qreal contentX)
{
    Q_D(QQuickTableView);
    if (qFuzzyCompare(d->contentPos.x(), contentX))
        return;

    d->contentPos.setX(contentX);
    d->updatePolish();
    emit contentXChanged();
}

void QQuickTableView::setContentY(qreal contentY)
{
    Q_D(QQuickTableView);
    if (qFuzzyCompare(d->contentPos.y(), contentY))
        return;

    d->contentPos.setY(contentY);
    d->updatePolish();
    emit contentYChanged();
}

QQuickFlickable *QQuickTableView::flickable() const
{
    Q_D(const QQuickTableView);
    if (!d->flickable) {
        QQuickTableView *self = const_cast<QQuickTableView *>(this);
        self->d_func()->createFlickable();
    }

    return d->flickable;
}

void QQuickTableView::initItem(int index, QObject *object)
{
    Q_UNUSED(index);
    // Setting the view from the FxTableViewItem wrapper is too late if the delegate
    // needs access to the view in Component.onCompleted
    QQuickItem *item = qmlobject_cast<QQuickItem *>(object);
    if (!item)
        return;

    QObject *attachedObject = qmlAttachedPropertiesObject<QQuickTableView>(item);
    if (QQuickTableViewAttached *attached = static_cast<QQuickTableViewAttached *>(attachedObject))
        attached->setView(this);
}

void QQuickTableView::componentComplete()
{
    Q_D(QQuickTableView);

    if (d->interactive && !d->flickable)
        d->createFlickable();
    if (d->contentItem.isNull)
        d->contentItem = this;

    if (d->model && d->ownModel) {
        QQmlDelegateModel *delegateModel = static_cast<QQmlDelegateModel *>(d->model.data());
        if (!d->rowCount.isNull)
            delegateModel->setRows(d->rowCount);
        if (!d->columnCount.isNull)
            delegateModel->setColumns(d->columnCount);
        static_cast<QQmlDelegateModel *>(d->model.data())->componentComplete();
    }


    QQuickItem::componentComplete();
}

QT_END_NAMESPACE
