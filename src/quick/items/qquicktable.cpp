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

#include "qquicktable_p.h"

#include <QtCore/qtimer.h>
#include <QtQml/private/qqmldelegatemodel_p.h>
#include <QtQml/private/qqmlincubator_p.h>
#include <QtQml/private/qqmlchangeset_p.h>
#include <QtQml/qqmlinfo.h>

#include "qquickflickable_p.h"
#include "qquickrectangle_p.h"
#include "qquickitemviewfxitem_p_p.h"

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcTableDelegateLifecycle, "qt.quick.table.lifecycle")

#define Q_TABLE_UNREACHABLE(output) { dumpTable(); qWarning() << "output:" << output; Q_UNREACHABLE(); }
#define Q_TABLE_ASSERT(cond, output) Q_ASSERT(cond || [&](){ dumpTable(); qWarning() << "output:" << output; return false;}())

static const Qt::Edge allTableEdges[] = { Qt::LeftEdge, Qt::RightEdge, Qt::TopEdge, Qt::BottomEdge };
static const int kBufferTimerInterval = 300;
static const int kDefaultCacheBuffer = 300;

class FxTableItem;

static QString incubationModeToString(QQmlIncubator::IncubationMode mode)
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

class QQuickTablePrivate : public QQuickItemPrivate, public QQuickItemChangeListener
{
    Q_DECLARE_PUBLIC(QQuickTable)

public:
    QQuickTablePrivate();

    static inline QQuickTablePrivate *get(QQuickTable *q) { return q->d_func(); }

    void updatePolish() override;

    class TableSectionLoadRequest
    {
    public:
        QLine itemsToLoad;
        Qt::Edge edgeToLoad;
        int loadIndex;
        int loadCount;
        bool active = false;
        QQmlIncubator::IncubationMode incubationMode;
    };

protected:
    QList<FxTableItem *> loadedItems;

    QQmlInstanceModel* model;
    QVariant modelVariant;

    // loadedTable describes the table cells that are currently loaded (from top left
    // row/column to bottom right row/column). loadedTableRect describes the actual
    // pixels that those cells covers, and is used to determine if we need to
    // load more rows/columns. loadedTableRectInside describes the pixels that the loaded
    // table covers if you remove one row/column on each side of the table, and is used
    // to determine if we need to unload hidden rows/columns.
    // loadedTableRectWithUnloadMargins is just a helper rect that is used when the table
    // is flicked all the way to the end, where we then use it to be a bit more careful
    // about unloading edges for optimization reasons.
    QRect loadedTable;
    QRectF loadedTableRect;
    QRectF loadedTableRectInside;
    QRectF loadedTableRectWithUnloadMargins;

    QRectF viewportRect;
    QRectF bufferRect;

    QQmlNullableValue<int> rowCount;
    QQmlNullableValue<int> columnCount;

    TableSectionLoadRequest loadRequest;

    QPoint implicitSizeBenchMarkPoint;
    QSizeF spacing;

    int buffer;
    QTimer loadBufferDelayTimer;

    bool hasBufferedItems;
    bool blockItemCreatedCallback;

    bool tableInvalid;
    bool layoutInvalid;

    QVector<QMetaObject::Connection> containerConnections;
    QVector<QSizeF> cachedColumnAndRowSizes;

#ifdef QT_DEBUG
    QString forcedIncubationMode;
#endif

    const static QPoint kLeft;
    const static QPoint kRight;
    const static QPoint kUp;
    const static QPoint kDown;

protected:
    bool isRightToLeft() const;
    bool isBottomToTop() const;

    inline QQmlDelegateModel *wrapperModel() { return qobject_cast<QQmlDelegateModel*>(model); }
    inline QQmlDelegateModel *wrapperModel() const { return qobject_cast<QQmlDelegateModel*>(model); }

    inline int lineLength(const QLine &line);

    inline int rowAtIndex(int index) const;
    inline int columnAtIndex(int index) const;

    inline int indexAt(const QPoint &cellCoord) const;
    inline int modelIndexAt(const QPoint &cellCoord);

    inline QPoint coordAt(int index) const;
    inline QPoint coordAt(const FxTableItem *tableItem) const;
    inline QPoint coordAt(const QLine &line, int index);

    FxTableItem *loadedTableItem(int modelIndex) const;
    inline FxTableItem *loadedTableItem(const QPoint &cellCoord) const;
    inline FxTableItem *itemNextTo(const FxTableItem *fxTableItem, const QPoint &direction) const;

    QLine rectangleEdge(const QRect &rect, Qt::Edge tableEdge);
    QRect expandedRect(const QRect &rect, Qt::Edge edge, int increment);

    qreal columnWidth(int column);
    qreal rowHeight(int row);

    void relayoutAllItems();
    void layoutVerticalEdge(Qt::Edge tableEdge, bool adjustSize);
    void layoutHorizontalEdge(Qt::Edge tableEdge, bool adjustSize);
    void layoutLoadedTableEdge();

    void resetCachedColumnAndRowSizes();
    void enforceFirstRowColumnAtOrigo();
    void updateImplicitSize();
    void syncLoadedTableRectFromLoadedTable();
    void syncLoadedTableFromLoadRequest();

    bool canLoadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const;
    bool canUnloadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const;

    FxTableItem *createFxTableItem(const QPoint &cellCoord, QQmlIncubator::IncubationMode incubationMode);
    FxTableItem *loadFxTableItem(const QPoint &cellCoord, QQmlIncubator::IncubationMode incubationMode);
    bool releaseItem(FxTableItem *fxTableItem);
    void releaseLoadedItems();
    void clear();

    void unloadItem(const QPoint &cell);
    void unloadItems(const QLine &items);

    void loadInitialTopLeftItem();
    void loadEdgesInsideRect(const QRectF &fillRect, QQmlIncubator::IncubationMode incubationMode);
    void unloadEdgesOutsideRect(const QRectF &rect);
    void loadAndUnloadVisibleEdges();
    void cancelLoadRequest();
    void processLoadRequest();

    void loadBuffer();
    void unloadBuffer();

    void invalidateTable();
    void invalidateLayout();

    void createWrapperModel();
    void reconnectToContainer();

    inline QString tableLayoutToString() const;
    void dumpTable() const;
};

class FxTableItem : public QQuickItemViewFxItem
{
public:
    FxTableItem(QQuickItem *item, QQuickTable *table, bool own)
        : QQuickItemViewFxItem(item, own, QQuickTablePrivate::get(table))
        , table(table)
        , attached(static_cast<QQuickTableAttached*>(qmlAttachedPropertiesObject<QQuickTable>(item)))
    {
        if (attached)
            attached->setTable(table);
    }

    qreal position() const override { return 0; }
    qreal endPosition() const override { return 0; }
    qreal size() const override { return 0; }
    qreal sectionSize() const override { return 0; }
    bool contains(qreal, qreal) const override { return false; }

    QQuickTable *table;
    QQuickTableAttached *attached;
};

#ifndef QT_NO_DEBUG_STREAM
static QDebug operator<<(QDebug dbg, const QQuickTablePrivate::TableSectionLoadRequest request) {
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

const QPoint QQuickTablePrivate::kLeft = QPoint(-1, 0);
const QPoint QQuickTablePrivate::kRight = QPoint(1, 0);
const QPoint QQuickTablePrivate::kUp = QPoint(0, -1);
const QPoint QQuickTablePrivate::kDown = QPoint(0, 1);

QQuickTablePrivate::QQuickTablePrivate()
    : QQuickItemPrivate()
    , model(nullptr)
    , loadedTable(QRect())
    , loadedTableRect(QRectF())
    , loadedTableRectInside(QRectF())
    , viewportRect(QRectF(0, 0, -1, -1))
    , bufferRect(QRectF())
    , rowCount(QQmlNullableValue<int>())
    , columnCount(QQmlNullableValue<int>())
    , loadRequest(TableSectionLoadRequest())
    , implicitSizeBenchMarkPoint(QPoint(-1, -1))
    , spacing(QSize())
    , buffer(kDefaultCacheBuffer)
    , hasBufferedItems(false)
    , blockItemCreatedCallback(false)
    , tableInvalid(true)
    , layoutInvalid(true)
    , containerConnections(QVector<QMetaObject::Connection>(4))
#ifdef QT_DEBUG
    , forcedIncubationMode(qEnvironmentVariable("QT_TABLE_INCUBATION_MODE"))
#endif
{
    loadBufferDelayTimer.setSingleShot(true);
    QObject::connect(&loadBufferDelayTimer, &QTimer::timeout, [=]{ loadBuffer(); });
}

QString QQuickTablePrivate::tableLayoutToString() const
{
    return QString(QLatin1String("table cells: (%1,%2) -> (%3,%4), item count: %5, table rect: %6,%7 x %8,%9"))
            .arg(loadedTable.topLeft().x()).arg(loadedTable.topLeft().y())
            .arg(loadedTable.bottomRight().x()).arg(loadedTable.bottomRight().y())
            .arg(loadedItems.count())
            .arg(loadedTableRect.x())
            .arg(loadedTableRect.y())
            .arg(loadedTableRect.width())
            .arg(loadedTableRect.height());
}

void QQuickTablePrivate::dumpTable() const
{
    auto listCopy = loadedItems;
    std::stable_sort(listCopy.begin(), listCopy.end(),
                     [](const FxTableItem *lhs, const FxTableItem *rhs) { return lhs->index < rhs->index; });

    qWarning() << QStringLiteral("******* TABLE DUMP *******");
    for (int i = 0; i < listCopy.count(); ++i) {
        FxTableItem *item = static_cast<FxTableItem *>(listCopy.at(i));
        qWarning() << coordAt(item);
    }
    qWarning() << tableLayoutToString();

    QString filename = QStringLiteral("QQuickTable_dumptable_capture.png");
    if (q_func()->window()->grabWindow().save(filename))
        qWarning() << "Window capture saved to:" << filename;
}

int QQuickTablePrivate::lineLength(const QLine &line)
{
    // Note: line needs to be either vertical or horizontal
    return line.x2() - line.x1() + line.y2() - line.y1() + 1;
}

int QQuickTablePrivate::rowAtIndex(int index) const
{
    Q_Q(const QQuickTable);
    int rows = q->rows();
    if (index < 0 || rows <= 0)
        return -1;
    return index % rows;
}

int QQuickTablePrivate::columnAtIndex(int index) const
{
    Q_Q(const QQuickTable);
    int rows = q->rows();
    if (index < 0 || rows <= 0)
        return -1;
    return int(index / rows);
}

QPoint QQuickTablePrivate::coordAt(int index) const
{
    return QPoint(columnAtIndex(index), rowAtIndex(index));
}

QPoint QQuickTablePrivate::coordAt(const FxTableItem *tableItem) const
{
    return coordAt(tableItem->index);
}

QPoint QQuickTablePrivate::coordAt(const QLine &line, int index)
{
    // Note: a line is either vertical or horizontal
    int x = line.p1().x() + (line.dx() ? index : 0);
    int y = line.p1().y() + (line.dy() ? index : 0);
    return QPoint(x, y);
}

int QQuickTablePrivate::indexAt(const QPoint& cellCoord) const
{
    Q_Q(const QQuickTable);
    int rows = q->rows();
    int columns = q->columns();
    if (cellCoord.y() < 0 || cellCoord.y() >= rows || cellCoord.x() < 0 || cellCoord.x() >= columns)
        return -1;
    return cellCoord.y() + (cellCoord.x() * rows);
}

int QQuickTablePrivate::modelIndexAt(const QPoint &cellCoord)
{
    // The table can have a different row/column count than the model. Whenever
    // we need to reference an item in the model, we therefore need to map the
    // cellCoord in the table to the corresponding index in the model.

    if (auto wmodel = wrapperModel()) {
        int columnCountInModel = wmodel->columns();
        if (columnCountInModel > 1) {
            // The model has more than one column, and we therefore
            // treat it as a table. In that case, cellCoord needs
            // to be inside the table model to have a valid index.
            int rowCountInModel = wmodel->rows();
            if (cellCoord.x() >= columnCountInModel || cellCoord.y() >= rowCountInModel)
                return -1;
            return cellCoord.y() + (cellCoord.x() * rowCountInModel);
        }
    }

    // The model has zero or one column, and we therefore treat it as a list. For
    // lists, we assign model indices to cells by filling up rows first, then columns.
    int availableRows = !rowCount.isNull ? rowCount.value : model->count();
    int modelIndex = cellCoord.y() + (cellCoord.x() * availableRows);
    if (modelIndex >= model->count())
        return -1;
    return modelIndex;
}

void QQuickTablePrivate::resetCachedColumnAndRowSizes()
{
    Q_Q(QQuickTable);
    const int maxCacheSize = 100;
    cachedColumnAndRowSizes.clear();
    int rows = q->rows();
    int columns = q->columns();
    if (rows == 1 || columns == 1)
        cachedColumnAndRowSizes.squeeze();
    else
        cachedColumnAndRowSizes.resize(maxCacheSize);
}

void QQuickTablePrivate::enforceFirstRowColumnAtOrigo()
{
    // Gaps before the first row/column can happen if rows/columns
    // changes size while flicking e.g because of spacing changes or
    // changes to a column maxWidth/row maxHeight. Check for this, and
    // move the whole table rect accordingly.
    bool layoutNeeded = false;
    const qreal flickMargin = 50;

    if (loadedTable.x() == 0 && loadedTableRect.x() != 0) {
        // The table is at the beginning, but not at the edge of the
        // content view. So move the table to origo.
        loadedTableRect.moveLeft(0);
        layoutNeeded = true;
    } else if (loadedTableRect.x() < 0) {
        // The table is outside the beginning of the content view. Move
        // the whole table inside, and make some room for flicking.
        loadedTableRect.moveLeft(loadedTable.x() == 0 ? 0 : flickMargin);
        layoutNeeded = true;
    }

    if (loadedTable.y() == 0 && loadedTableRect.y() != 0) {
        loadedTableRect.moveTop(0);
        layoutNeeded = true;
    } else if (loadedTableRect.y() < 0) {
        loadedTableRect.moveTop(loadedTable.y() == 0 ? 0 : flickMargin);
        layoutNeeded = true;
    }

    if (layoutNeeded)
        relayoutAllItems();
}

void QQuickTablePrivate::updateImplicitSize()
{
    Q_Q(QQuickTable);

    const qreal thresholdBeforeAdjust = 0.1;
    int currentRightColumn = loadedTable.right();
    int currentBottomRow = loadedTable.bottom();

    if (currentRightColumn > implicitSizeBenchMarkPoint.x()) {
        implicitSizeBenchMarkPoint.setX(currentRightColumn);

        qreal newWidth = loadedTableRect.right();
        qreal averageCellSize = newWidth / (currentRightColumn + 1);
        qreal averageSize = averageCellSize + spacing.width();
        qreal estimatedWith = (q->columns() * averageSize) - spacing.width();

        if (currentRightColumn >= q->columns() - 1) {
            // We are at the last column, and can set the exact width
            if (newWidth != q->implicitWidth())
                q->setImplicitWidth(newWidth);
        } else if (newWidth >= q->implicitWidth()) {
            // We are at the estimated width, but there are still more columns
            q->setImplicitWidth(estimatedWith);
        } else {
            // Only set a new width if the new estimate is substantially different
            qreal diff = 1 - (estimatedWith / q->implicitWidth());
            if (qAbs(diff) > thresholdBeforeAdjust)
                q->setImplicitWidth(estimatedWith);
        }
    }

    if (currentBottomRow > implicitSizeBenchMarkPoint.y()) {
        implicitSizeBenchMarkPoint.setY(currentBottomRow);

        qreal newHeight = loadedTableRect.bottom();
        qreal averageCellSize = newHeight / (currentBottomRow + 1);
        qreal averageSize = averageCellSize + spacing.height();
        qreal estimatedHeight = (q->rows() * averageSize) - spacing.height();

        if (currentBottomRow >= q->rows() - 1) {
            // We are at the last row, and can set the exact height
            if (newHeight != q->implicitHeight())
                q->setImplicitHeight(newHeight);
        } else if (newHeight >= q->implicitHeight()) {
            // We are at the estimated height, but there are still more rows
            q->setImplicitHeight(estimatedHeight);
        } else {
            // Only set a new height if the new estimate is substantially different
            qreal diff = 1 - (estimatedHeight / q->implicitHeight());
            if (qAbs(diff) > thresholdBeforeAdjust)
                q->setImplicitHeight(estimatedHeight);
        }
    }
}

void QQuickTablePrivate::syncLoadedTableRectFromLoadedTable()
{
    Q_Q(QQuickTable);

    QRectF topLeftRect = loadedTableItem(loadedTable.topLeft())->geometry();
    QRectF bottomRightRect = loadedTableItem(loadedTable.bottomRight())->geometry();
    loadedTableRect = topLeftRect.united(bottomRightRect);
    loadedTableRectInside = QRectF(topLeftRect.bottomRight(), bottomRightRect.topLeft());

    // We maintain an additional rect that adds some extra margins to loadedTableRect when
    // the loadedTableRect is at the edge of the logical table. This is just convenient when
    // we in updatePolish() need to check if the table size should be trimmed to speed up
    // edge loading (since we then don't need to add explicit checks for this each time the
    // viewport moves). The margins ensure that we don't unload just because the user
    // overshoots when flicking.
    loadedTableRectWithUnloadMargins = loadedTableRect;

    if (loadedTableRect.x() == 0)
        loadedTableRectWithUnloadMargins.setLeft(-q->width());
    if (loadedTableRect.y() == 0)
        loadedTableRectWithUnloadMargins.setTop(-q->height());
    if (loadedTableRect.width() == q->implicitWidth())
        loadedTableRectWithUnloadMargins.setRight(q->implicitWidth() + q->width());
    if (loadedTableRect.y() == q->implicitHeight())
        loadedTableRectWithUnloadMargins.setBottom(q->implicitHeight() + q->height());
}

void QQuickTablePrivate::syncLoadedTableFromLoadRequest()
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

FxTableItem *QQuickTablePrivate::itemNextTo(const FxTableItem *fxTableItem, const QPoint &direction) const
{
    return loadedTableItem(coordAt(fxTableItem) + direction);
}

FxTableItem *QQuickTablePrivate::loadedTableItem(int modelIndex) const
{
    for (int i = 0; i < loadedItems.count(); ++i) {
        FxTableItem *item = loadedItems.at(i);
        if (item->index == modelIndex)
            return item;
    }

    Q_TABLE_UNREACHABLE(coordAt(modelIndex));
    return nullptr;
}

FxTableItem *QQuickTablePrivate::loadedTableItem(const QPoint &cellCoord) const
{
    return loadedTableItem(indexAt(cellCoord));
}

FxTableItem *QQuickTablePrivate::createFxTableItem(const QPoint &cellCoord, QQmlIncubator::IncubationMode incubationMode)
{
    Q_Q(QQuickTable);

    QQuickItem *item = nullptr;
    int modelIndex = modelIndexAt(cellCoord);

    if (modelIndex != -1) {
        // Requested cellCoord is covered by the model
        QObject* object = model->object(modelIndex, incubationMode);
        if (!object) {
            if (model->incubationStatus(modelIndex) == QQmlIncubator::Loading) {
                // Item is incubating. Return nullptr for now, and let the table call this
                // function again once we get a callback to itemCreatedCallback().
                return nullptr;
            }

            qWarning() << "Table: failed loading index:" << modelIndex;
            object = new QQuickItem();
        }

        item = qmlobject_cast<QQuickItem*>(object);
        if (!item) {
            qWarning() << "Table: delegate is not an item:" << modelIndex;
            delete object;
            // The model cannot provide an item for the given
            // index, so we create a placeholder instead.
            item = new QQuickItem();
        }
    } else {
        // The model cannot provide an item for the given
        // index, so we create a placeholder instead.
        item = new QQuickItem();
    }

    item->setParentItem(q);

    FxTableItem *fxTableItem = new FxTableItem(item, q, false);
    fxTableItem->setVisible(false);
    // Note: fxTableItem->index is the index in
    // the table, and not in the model.
    fxTableItem->index = indexAt(cellCoord);
    return fxTableItem;
}

FxTableItem *QQuickTablePrivate::loadFxTableItem(const QPoint &cellCoord, QQmlIncubator::IncubationMode incubationMode)
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
    QBoolBlocker guard(blockItemCreatedCallback);
    auto item = createFxTableItem(cellCoord, incubationMode);
    qCDebug(lcTableDelegateLifecycle) << cellCoord << "ready?" << bool(item);
    return item;
}

void QQuickTablePrivate::releaseLoadedItems() {
    // Make a copy and clear the list of items first to avoid destroyed
    // items being accessed during the loop (QTBUG-61294)
    const QList<FxTableItem *> tmpList = loadedItems;
    loadedItems.clear();
    for (FxTableItem *item : tmpList)
        releaseItem(item);
}

bool QQuickTablePrivate::releaseItem(FxTableItem *fxTableItem)
{
    if (fxTableItem->item) {
        if (model->release(fxTableItem->item) == QQmlInstanceModel::Destroyed)
            fxTableItem->item->setParentItem(nullptr);
    }

    delete fxTableItem;
    return flags != QQmlInstanceModel::Referenced;
}

void QQuickTablePrivate::clear()
{
    releaseLoadedItems();
    loadedTable = QRect();
    loadedTableRect = QRect();
    resetCachedColumnAndRowSizes();
    implicitSizeBenchMarkPoint = QPoint(-1, -1);
    updateImplicitSize();

    if (loadRequest.active) {
        model->cancel(loadRequest.loadIndex);
        loadRequest = TableSectionLoadRequest();
    }
}

void QQuickTablePrivate::unloadItem(const QPoint &cell)
{
    FxTableItem *item = loadedTableItem(cell);
    loadedItems.removeOne(item);
    releaseItem(item);
}

void QQuickTablePrivate::unloadItems(const QLine &items)
{
    qCDebug(lcTableDelegateLifecycle) << items;

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

QLine QQuickTablePrivate::rectangleEdge(const QRect &rect, Qt::Edge tableEdge)
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
    Q_TABLE_UNREACHABLE(tableEdge);
    return QLine();
}

QRect QQuickTablePrivate::expandedRect(const QRect &rect, Qt::Edge edge, int increment)
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
    Q_TABLE_UNREACHABLE(edge);
    return QRect();
}

bool QQuickTablePrivate::canLoadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const
{
    Q_Q(const QQuickTable);

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

bool QQuickTablePrivate::canUnloadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const
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
    Q_TABLE_UNREACHABLE(tableEdge);
    return false;
}

qreal QQuickTablePrivate::columnWidth(int column)
{
    qreal columnWidth = 0;
    if (column < cachedColumnAndRowSizes.size()) {
        columnWidth = cachedColumnAndRowSizes[column].width();
        if (columnWidth > 0)
            return columnWidth;
    }

    // Calculate the max width of all visible cells in the given column
    for (int row = loadedTable.top(); row <= loadedTable.bottom(); ++row) {
        auto const item = loadedTableItem(QPoint(column, row));
        columnWidth = qMax(columnWidth, item->geometry().width());
    }

    if (column < cachedColumnAndRowSizes.size())
        cachedColumnAndRowSizes[column].setWidth(columnWidth);
    return columnWidth;
}

qreal QQuickTablePrivate::rowHeight(int row)
{
    qreal rowHeight = 0;
    if (row < cachedColumnAndRowSizes.size()) {
        rowHeight = cachedColumnAndRowSizes[row].height();
        if (rowHeight > 0)
            return rowHeight;
    }

    // Calculate the max height of all visible cells in the given row
    for (int column = loadedTable.left(); column <= loadedTable.right(); ++column) {
        const auto item = loadedTableItem(QPoint(column, row));
        rowHeight = qMax(rowHeight, item->geometry().height());
    }

    if (row < cachedColumnAndRowSizes.size())
        cachedColumnAndRowSizes[row].setHeight(rowHeight);
    return rowHeight;
}

void QQuickTablePrivate::relayoutAllItems()
{
    qCDebug(lcTableDelegateLifecycle);
    layoutInvalid = false;

    qreal nextColumnX = loadedTableRect.x();
    qreal nextRowY = loadedTableRect.y();

    for (int column = loadedTable.left(); column <= loadedTable.right(); ++column) {
        // Adjust the geometry of all cells in the current column
        qreal width = columnWidth(column);
        for (int row = loadedTable.top(); row <= loadedTable.bottom(); ++row) {
            auto item = loadedTableItem(QPoint(column, row));
            QRectF geometry = item->geometry();
            geometry.moveLeft(nextColumnX);
            geometry.setWidth(width);
            item->setGeometry(geometry);
        }

        nextColumnX += width + spacing.width();
    }

    for (int row = loadedTable.top(); row <= loadedTable.bottom(); ++row) {
        // Adjust the geometry of all cells in the current row
        qreal height = rowHeight(row);
        for (int column = loadedTable.left(); column <= loadedTable.right(); ++column) {
            auto item = loadedTableItem(QPoint(column, row));
            QRectF geometry = item->geometry();
            geometry.moveTop(nextRowY);
            geometry.setHeight(height);
            item->setGeometry(geometry);
        }

        nextRowY += height + spacing.height();
    }

    if (Q_UNLIKELY(lcTableDelegateLifecycle().isDebugEnabled())) {
        for (int column = loadedTable.left(); column <= loadedTable.right(); ++column) {
            for (int row = loadedTable.top(); row <= loadedTable.bottom(); ++row) {
                QPoint cell = QPoint(column, row);
                qCDebug(lcTableDelegateLifecycle()) << "relayout item:" << cell << loadedTableItem(cell)->geometry();
            }
        }
    }

    syncLoadedTableRectFromLoadedTable();
    implicitSizeBenchMarkPoint = QPoint(-1, -1);
    updateImplicitSize();
}

void QQuickTablePrivate::layoutVerticalEdge(Qt::Edge tableEdge, bool adjustSize)
{
    int column = (tableEdge == Qt::LeftEdge) ? loadedTable.left() : loadedTable.right();
    QPoint neighbourCell = (tableEdge == Qt::LeftEdge) ? kRight : kLeft;
    qreal width = adjustSize ? columnWidth(column) : 0;

    for (int row = loadedTable.top(); row <= loadedTable.bottom(); ++row) {
        auto fxTableItem = loadedTableItem(QPoint(column, row));
        auto const neighbourItem = itemNextTo(fxTableItem, neighbourCell);

        qreal w = 0;
        qreal h = 0;

        if (adjustSize) {
            w = width;
            h = neighbourItem->geometry().height();
        }

        if (w <= 0)
            w = fxTableItem->geometry().width();

        if (h <= 0)
            h = fxTableItem->geometry().height();

        qreal x = (tableEdge == Qt::LeftEdge) ?
            neighbourItem->geometry().left() - spacing.width() - w :
            neighbourItem->geometry().right() + spacing.width();
        qreal y = neighbourItem->geometry().top();

        fxTableItem->setGeometry(QRectF(x, y, w, h));
        fxTableItem->setVisible(true);

        qCDebug(lcTableDelegateLifecycle()) << "layout item:"
                                            << QPoint(column, row)
                                            << fxTableItem->geometry()
                                            << "adjust size:" << adjustSize;
    }
}

void QQuickTablePrivate::layoutHorizontalEdge(Qt::Edge tableEdge, bool adjustSize)
{
    int row = (tableEdge == Qt::TopEdge) ? loadedTable.top() : loadedTable.bottom();
    QPoint neighbourCell = (tableEdge == Qt::TopEdge) ? kDown : kUp;
    qreal height = adjustSize ? rowHeight(row) : 0;

    for (int column = loadedTable.left(); column <= loadedTable.right(); ++column) {
        auto fxTableItem = loadedTableItem(QPoint(column, row));
        auto const neighbourItem = itemNextTo(fxTableItem, neighbourCell);

        qreal w = 0;
        qreal h = 0;

        if (adjustSize) {
            w = neighbourItem->geometry().width();
            h = height;
        }

        if (w <= 0)
            w = fxTableItem->geometry().width();

        if (h <= 0)
            h = fxTableItem->geometry().height();

        qreal x = neighbourItem->geometry().left();
        qreal y = (tableEdge == Qt::TopEdge) ?
            neighbourItem->geometry().top() - spacing.height() - h :
            neighbourItem->geometry().bottom() + spacing.height();

        fxTableItem->setGeometry(QRectF(x, y, w, h));
        fxTableItem->setVisible(true);

        qCDebug(lcTableDelegateLifecycle()) << "layout item:"
                                            << QPoint(column, row)
                                            << fxTableItem->geometry()
                                            << "adjust size:" << adjustSize;
    }
}

void QQuickTablePrivate::layoutLoadedTableEdge()
{
    // If a relayout is pending, we avoid adjusting cell sizes
    // until all items have been loaded, so we can do a full
    // layout based on the items original sizes.
    const bool adjustSize = !layoutInvalid;

    switch (loadRequest.edgeToLoad) {
    case Qt::LeftEdge:
    case Qt::RightEdge:
        layoutVerticalEdge(loadRequest.edgeToLoad, adjustSize);
        break;
    case Qt::TopEdge:
    case Qt::BottomEdge:
        layoutHorizontalEdge(loadRequest.edgeToLoad, adjustSize);
        break;
    default:
        // Request loaded top-left item rather than an edge.
        // ###todo: support starting with other top-left items than 0,0
        Q_TABLE_ASSERT(loadRequest.itemsToLoad.p1() == QPoint(0, 0), loadRequest);
        loadedTableItem(QPoint(0, 0))->setVisible(true);
        qCDebug(lcTableDelegateLifecycle) << "top-left item geometry:" << loadedTableItem(QPoint(0, 0))->geometry();
        break;
    }
}

void QQuickTablePrivate::cancelLoadRequest()
{
    loadRequest.active = false;
    model->cancel(loadRequest.loadIndex);

    if (tableInvalid) {
        // No reason to rollback already loaded edge items
        // since we anyway are about to reload all items.
        return;
    }

    const int lastLoadedIndex = loadRequest.loadIndex - 1;
    if (lastLoadedIndex < 0)
        return;

    QLine rollbackItems;
    rollbackItems.setP1(loadRequest.itemsToLoad.p1());
    rollbackItems.setP2(coordAt(loadRequest.itemsToLoad, lastLoadedIndex));
    qCDebug(lcTableDelegateLifecycle()) << "rollback:" << rollbackItems << tableLayoutToString();
    unloadItems(rollbackItems);

    loadRequest = TableSectionLoadRequest();
}

void QQuickTablePrivate::processLoadRequest()
{
    if (!loadRequest.active) {
        loadRequest.active = true;
        if (Qt::Edge edge = loadRequest.edgeToLoad)
            loadRequest.itemsToLoad = rectangleEdge(expandedRect(loadedTable, edge, 1), edge);
        loadRequest.loadCount = lineLength(loadRequest.itemsToLoad);
        qCDebug(lcTableDelegateLifecycle()) << "begin:" << loadRequest;
    }

    for (; loadRequest.loadIndex < loadRequest.loadCount; ++loadRequest.loadIndex) {
        QPoint cell = coordAt(loadRequest.itemsToLoad, loadRequest.loadIndex);
        FxTableItem *fxTableItem = loadFxTableItem(cell, loadRequest.incubationMode);

        if (!fxTableItem) {
            // Requested item is not yet ready. Just leave, and wait for this
            // function to be called again when the item is ready.
            return;
        }

        loadedItems.append(fxTableItem);
    }

    // Load request done
    syncLoadedTableFromLoadRequest();
    layoutLoadedTableEdge();
    syncLoadedTableRectFromLoadedTable();
    enforceFirstRowColumnAtOrigo();
    updateImplicitSize();

    loadRequest = TableSectionLoadRequest();
    qCDebug(lcTableDelegateLifecycle()) << "completed after:" << tableLayoutToString();
}

void QQuickTablePrivate::loadInitialTopLeftItem()
{
    Q_Q(QQuickTable);
    Q_TABLE_ASSERT(loadedItems.isEmpty(), "");

    if (q->rows() == 0 || q->columns() == 0)
        return;

    // Load top-left item. After loaded, loadItemsInsideRect() will take
    // care of filling out the rest of the table.
    QPoint topLeft(0, 0);
    loadRequest.itemsToLoad = QLine(topLeft, topLeft);
    loadRequest.incubationMode = QQmlIncubator::AsynchronousIfNested;
    qCDebug(lcTableDelegateLifecycle()) << loadRequest;
    processLoadRequest();
}

void QQuickTablePrivate::unloadEdgesOutsideRect(const QRectF &rect)
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
                qCDebug(lcTableDelegateLifecycle) << tableLayoutToString();
            }
        }

    } while (continueUnloadingEdges);
}

void QQuickTablePrivate::loadEdgesInsideRect(const QRectF &fillRect, QQmlIncubator::IncubationMode incubationMode)
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

void QQuickTablePrivate::loadAndUnloadVisibleEdges()
{
    // Unload table edges that have been moved outside the visible part of the
    // table (including buffer area), and load new edges that has been moved inside.
    // Note: an important point is that we always keep the table rectangular
    // and without holes to reduce complexity (we never leave the table in
    // a half-loaded state, or keep track of multiple patches).
    // We load only one edge (row or column) at a time. This is especially
    // important when loading into the buffer, since we need to be able to
    // cancel the buffering quickly if the user starts to flick, and then
    // focus all further loading on the edges that are flicked into view.

    if (loadRequest.active) {
        // Don't start loading more edges while we're
        // already aiting for another one to load.
        return;
    }

    if (loadedItems.isEmpty()) {
        // We need at least the top-left item to be loaded before we can
        // start loading edges around it. Not having a top-left item at
        // this point means that the model is empty (or row and and column
        // is 0) since we have no active load request.
        return;
    }

    unloadEdgesOutsideRect(hasBufferedItems ? bufferRect : viewportRect);
    loadEdgesInsideRect(viewportRect, QQmlIncubator::AsynchronousIfNested);
}

void QQuickTablePrivate::loadBuffer()
{
    // Rather than making sure to stop the timer from all locations that can
    // violate the "buffering allowed" state, we just check that we're in the
    // right state here before we start buffering.
    if (buffer <= 0 || loadRequest.active || loadedItems.isEmpty())
        return;

    qCDebug(lcTableDelegateLifecycle());
    loadEdgesInsideRect(bufferRect, QQmlIncubator::Asynchronous);
    hasBufferedItems = true;
}

void QQuickTablePrivate::unloadBuffer()
{
    if (!hasBufferedItems)
        return;

    qCDebug(lcTableDelegateLifecycle());
    hasBufferedItems = false;
    loadBufferDelayTimer.stop();
    cancelLoadRequest();
    unloadEdgesOutsideRect(viewportRect);
}

void QQuickTablePrivate::invalidateTable() {
    tableInvalid = true;
    if (loadRequest.active)
        cancelLoadRequest();
    q_func()->polish();
}

void QQuickTablePrivate::invalidateLayout() {
    layoutInvalid = true;
    q_func()->polish();
}

void QQuickTablePrivate::updatePolish()
{
    if (loadRequest.active) {
        // We're loading items async, so don't continue
        // until all requested items have been received.
        return;
    }

    if (!viewportRect.isValid())
        return;

    bufferRect = viewportRect.adjusted(-buffer, -buffer, buffer, buffer);

    if (tableInvalid) {
        clear();
        tableInvalid = false;
        layoutInvalid = true;
        loadInitialTopLeftItem();
        loadAndUnloadVisibleEdges();
        if (loadRequest.active)
            return;
    }

    if (loadedItems.isEmpty()) {
        qCDebug(lcTableDelegateLifecycle()) << "no items loaded, meaning empty model or row/column == 0";
        return;
    }

    if (layoutInvalid)
        relayoutAllItems();

    if (hasBufferedItems && !loadedTableRectWithUnloadMargins.contains(viewportRect)) {
        // The visible rect has passed the loaded table rect, including buffered items. This
        // means that we need to load new edges immediately to avoid flicking in empty areas.
        // Since we always keep the table rectangular, we trim the table down to be the size of
        // the visible rect so that each edge contains fewer items. This will make it faster to
        // load new edges as the viewport moves.
        unloadBuffer();
    }

    loadAndUnloadVisibleEdges();
    if (loadRequest.active)
        return;

    if (buffer > 0) {
        // When polish hasn't been called for a while (meaning that the viewport rect hasn't
        // changed), we start buffering items. We delay this operation by using a timer to
        // increase performance (by not loading hidden items) while the user is flicking.
        loadBufferDelayTimer.start(kBufferTimerInterval);
    }
}

void QQuickTablePrivate::createWrapperModel()
{
    Q_Q(QQuickTable);

    auto delegateModel = new QQmlDelegateModel(qmlContext(q), q);
    if (q->isComponentComplete())
        delegateModel->componentComplete();
    model = delegateModel;
}

void QQuickTablePrivate::reconnectToContainer()
{
    Q_Q(QQuickTable);

    for (auto connection : containerConnections)
        q->disconnect(connection);
    containerConnections.clear();

    QQuickItem *parent = q->parentItem();
    if (!parent)
        return;

    QQuickItem *nextParent = parent;
    QQuickItem *container = parent;
    QQuickItem *contentItem = nullptr;

    while (nextParent) {
        if (auto flickable = qobject_cast<QQuickFlickable *>(nextParent)) {
            container = flickable;
            contentItem = flickable->contentItem();
            break;
        }
        nextParent = nextParent->parentItem();
    }

    qCDebug(lcTableDelegateLifecycle) << "connecting to container:" << container << "using contentItem:" << contentItem;
    viewportRect = QRectF(0, 0, container->width(), container->height());

    containerConnections << q->connect(container, &QQuickItem::widthChanged, [=]{
        viewportRect.setWidth(container->width());
        updatePolish();
    });

    containerConnections << q->connect(container, &QQuickItem::heightChanged, [=]{
        viewportRect.setHeight(container->height());
        updatePolish();
    });

    if (contentItem) {
        QPointF offset = q->mapToItem(contentItem, QPoint(0, 0));
        viewportRect.moveLeft(-contentItem->x() - offset.x());
        viewportRect.moveTop(-contentItem->y() - offset.y());

        containerConnections << q->connect(contentItem, &QQuickItem::xChanged, [=]{
            QPointF offset = q->mapToItem(contentItem, QPoint(0, 0));
            viewportRect.moveLeft(-contentItem->x() - offset.x());
            updatePolish();
        });

        containerConnections << q->connect(contentItem, &QQuickItem::yChanged, [=]{
            QPointF offset = q->mapToItem(contentItem, QPoint(0, 0));
            viewportRect.moveTop(-contentItem->y() - offset.y());
            updatePolish();
        });
    }
}

QQuickTable::QQuickTable(QQuickItem *parent)
    : QQuickItem(*(new QQuickTablePrivate), parent)
{
}

int QQuickTable::rows() const
{
    Q_D(const QQuickTable);
    if (!d->rowCount.isNull)
        return d->rowCount;
    if (auto wrapperModel = d->wrapperModel())
        return wrapperModel->rows();
    if (d->model)
        return d->model->count();
    return 0;
}

void QQuickTable::setRows(int rows)
{
    Q_D(QQuickTable);
    if (d->rowCount == rows || rows < 0)
        return;

    d->rowCount = rows;
    d->invalidateTable();
    emit rowsChanged();
}

void QQuickTable::resetRows()
{
    Q_D(QQuickTable);
    if (d->rowCount.isNull)
        return;

    d->rowCount = QQmlNullableValue<int>();
    d->invalidateTable();
    emit rowsChanged();
}

int QQuickTable::columns() const
{
    Q_D(const QQuickTable);
    if (!d->columnCount.isNull)
        return d->columnCount;
    if (auto wrapperModel = d->wrapperModel())
        return wrapperModel->columns();
    return 1;
}

void QQuickTable::setColumns(int columns)
{
    Q_D(QQuickTable);
    if (d->columnCount == columns || columns < 0)
        return;

    d->columnCount = columns;
    d->invalidateTable();
    emit columnsChanged();
}

void QQuickTable::resetColumns()
{
    Q_D(QQuickTable);
    if (d->columnCount.isNull)
        return;

    d->columnCount = QQmlNullableValue<int>();
    d->invalidateTable();
    emit columnsChanged();
}

qreal QQuickTable::rowSpacing() const
{
    return d_func()->spacing.height();
}

void QQuickTable::setRowSpacing(qreal spacing)
{
    Q_D(QQuickTable);
    if (qFuzzyCompare(d->spacing.height(), spacing))
        return;

    d->spacing.setHeight(spacing);
    d->invalidateLayout();
    emit rowSpacingChanged();
}

qreal QQuickTable::columnSpacing() const
{
    return d_func()->spacing.width();
}

void QQuickTable::setColumnSpacing(qreal spacing)
{
    Q_D(QQuickTable);
    if (qFuzzyCompare(d->spacing.width(), spacing))
        return;

    d->spacing.setWidth(spacing);
    d->invalidateLayout();
    emit columnSpacingChanged();
}

int QQuickTable::cacheBuffer() const
{
    return d_func()->buffer;
}

void QQuickTable::setCacheBuffer(int newBuffer)
{
    Q_D(QQuickTable);
    if (d->buffer == newBuffer || newBuffer < 0)
        return;

    d->buffer = newBuffer;

    if (newBuffer == 0)
        d->unloadBuffer();

    emit cacheBufferChanged();
    polish();
}

QVariant QQuickTable::model() const
{
    return d_func()->modelVariant;
}

void QQuickTable::setModel(const QVariant &newModel)
{
    Q_D(QQuickTable);
    int prevRows = rows();
    int prevColumns = columns();

    d->modelVariant = newModel;
    QVariant effectiveModelVariant = d->modelVariant;
    if (effectiveModelVariant.userType() == qMetaTypeId<QJSValue>())
        effectiveModelVariant = effectiveModelVariant.value<QJSValue>().toVariant();

    if (d->model)
        disconnect(d->model, 0, this, 0);

    const auto instanceModel = qobject_cast<QQmlInstanceModel *>(qvariant_cast<QObject*>(effectiveModelVariant));

    if (instanceModel) {
        if (d->wrapperModel())
            delete d->model;
        d->model = instanceModel;
    } else {
        if (!d->wrapperModel())
            d->createWrapperModel();
        d->wrapperModel()->setModel(effectiveModelVariant);
    }

    connect(d->model, &QQmlInstanceModel::createdItem, this, &QQuickTable::itemCreatedCallback);
    connect(d->model, &QQmlInstanceModel::initItem, this, &QQuickTable::initItemCallback);
    connect(d->model, &QQmlInstanceModel::modelUpdated, this, &QQuickTable::modelUpdated);

    d->invalidateTable();

    emit modelChanged();
    if (prevRows != rows())
        emit rowsChanged();
    if (prevColumns != columns())
        emit columnsChanged();
}

QQmlComponent *QQuickTable::delegate() const
{
    if (QQmlDelegateModel *delegateModel = qobject_cast<QQmlDelegateModel*>(d_func()->model))
        return delegateModel->delegate();

    return nullptr;
}

void QQuickTable::setDelegate(QQmlComponent *newDelegate)
{
    Q_D(QQuickTable);
    if (newDelegate == delegate())
        return;

    if (!d->wrapperModel())
        d->createWrapperModel();

    d->wrapperModel()->setDelegate(newDelegate);
    d->invalidateTable();

    emit delegateChanged();
}

QQuickTableAttached *QQuickTable::qmlAttachedProperties(QObject *obj)
{
    return new QQuickTableAttached(obj);
}

void QQuickTable::itemCreatedCallback(int modelIndex, QObject*)
{
    Q_D(QQuickTable);

    if (d->blockItemCreatedCallback)
        return;

    qCDebug(lcTableDelegateLifecycle) << "item received async, model index:" << modelIndex;

    d->processLoadRequest();
    d->loadAndUnloadVisibleEdges();
    d->updatePolish();
}

void QQuickTable::initItemCallback(int modelIndex, QObject *object)
{
    Q_UNUSED(modelIndex);
    QQuickItem *item = qmlobject_cast<QQuickItem *>(object);
    if (!item)
        return;

    // Setting the view from the FxTableItem wrapper is too late if
    // the delegate needs access to the table in Component.onCompleted.
    QObject *attachedObject = qmlAttachedPropertiesObject<QQuickTable>(item);
    if (QQuickTableAttached *attached = static_cast<QQuickTableAttached *>(attachedObject))
        attached->setTable(this);
}

void QQuickTable::modelUpdated(const QQmlChangeSet &changeSet, bool reset)
{
    Q_UNUSED(changeSet);
    Q_UNUSED(reset);

    if (!isComponentComplete())
        return;

    // Rudimentary solution for now until support for
    // more fine-grained updates and transitions are implemented.
    auto modelVariant = model();
    setModel(QVariant());
    setModel(modelVariant);
}

void QQuickTable::componentComplete()
{
    Q_D(QQuickTable);

    if (!d->model)
        setModel(QVariant());

    if (auto wrapperModel = d->wrapperModel())
        wrapperModel->componentComplete();

    connect(this, &QQuickTable::parentChanged, [=]{ d->reconnectToContainer(); });
    d->reconnectToContainer();

    QQuickItem::componentComplete();
}

#include "moc_qquicktable_p.cpp"

QT_END_NAMESPACE
