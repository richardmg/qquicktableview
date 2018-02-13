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

static QLine rectangleEdge(const QRect &rect, Qt::Edge tableEdge)
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
    return QLine();
}

static QRect expandedRect(const QRect &rect, Qt::Edge edge, int increment)
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
    return QRect();
}

class FxTableItem;

class QQuickTablePrivate : public QQuickItemPrivate, public QQuickItemChangeListener
{
    Q_DECLARE_PUBLIC(QQuickTable)

public:
    class TableEdgeLoadRequest
    {
        // Whenever we need to load new rows or columns in the
        // table, we fill out a TableEdgeLoadRequest.
        // TableEdgeLoadRequest is just a struct that keeps track
        // of which cells that needs to be loaded, and which cell
        // the table is currently loading. The loading itself is
        // done by the table.

    public:
        void begin(const QPoint &cell, QQmlIncubator::IncubationMode incubationMode)
        {
            Q_ASSERT(!active);
            active = true;
            tableEdge = Qt::Edge(0);
            tableCells = QLine(cell, cell);
            mode = incubationMode;
            cellCount = 1;
            currentIndex = 0;
            qCDebug(lcTableDelegateLifecycle()) << "begin top-left:" << toString();
        }

        void begin(const QLine cellsToLoad, Qt::Edge edgeToLoad, QQmlIncubator::IncubationMode incubationMode)
        {
            Q_ASSERT(!active);
            active = true;
            tableEdge = edgeToLoad;
            tableCells = cellsToLoad;
            mode = incubationMode;
            cellCount = tableCells.x2() - tableCells.x1() + tableCells.y2() - tableCells.y1() + 1;
            currentIndex = 0;
            qCDebug(lcTableDelegateLifecycle()) << "begin:" << toString();
        }

        inline void markAsDone() { active = false; }
        inline bool isActive() { return active; }

        inline QPoint firstCell() { return tableCells.p1(); }
        inline QPoint lastCell() { return tableCells.p2(); }
        inline QPoint currentCell() { return cellAt(currentIndex); }
        inline QPoint previousCell() { return cellAt(currentIndex - 1); }

        inline bool atBeginning() { return currentIndex == 0; }
        inline bool hasCurrentCell() { return currentIndex < cellCount; }
        inline void moveToNextCell() { ++currentIndex; }

        inline Qt::Edge edge() { return tableEdge; }
        inline QQmlIncubator::IncubationMode incubationMode() { return mode; }

        QString toString()
        {
            QString str;
            QDebug dbg(&str);
            dbg.nospace() << "TableSectionLoadRequest(" << "edge:"
                << tableEdge << " cells:" << tableCells << " incubation:";

            switch (mode) {
            case QQmlIncubator::Asynchronous:
                dbg << "Asynchronous";
                break;
            case QQmlIncubator::AsynchronousIfNested:
                dbg << "AsynchronousIfNested";
                break;
            case QQmlIncubator::Synchronous:
                dbg << "Synchronous";
                break;
            }

            return str;
        }

    private:
        Qt::Edge tableEdge = Qt::Edge(0);
        QLine tableCells;
        int currentIndex = 0;
        int cellCount = 0;
        bool active = false;
        QQmlIncubator::IncubationMode mode = QQmlIncubator::AsynchronousIfNested;

        QPoint cellAt(int index)
        {
            int x = tableCells.p1().x() + (tableCells.dx() ? index : 0);
            int y = tableCells.p1().y() + (tableCells.dy() ? index : 0);
            return QPoint(x, y);
        }
    };

    struct ColumnRowSize
    {
        // ColumnRowSize is a helper class for storing row heights
        // and column widths. We calculate the average width of a column
        // the first time it's scrolled into view based on the size of
        // the loaded items in the new column. Since we never load more items
        // that what fits inside the viewport (cachebuffer aside), this calculation
        // would be different depending on which row you were at when the column
        // was scrolling in. To avoid that a column resizes when it's scrolled
        // in and out and in again, we store its width. But to avoid storing
        // the width for all columns, we choose to only store the width if it
        // differs from the column(s) to the left. The same logic applies for row heights.
        // 'index' translates to either 'row' or 'column'.
        int index;
        qreal size;

        static bool lessThan(const ColumnRowSize& a, const ColumnRowSize& b)
        {
            return a.index < b.index;
        }
    };

public:
    QQuickTablePrivate();

    static inline QQuickTablePrivate *get(QQuickTable *q) { return q->d_func(); }

    void updatePolish() override;

protected:
    QList<FxTableItem *> loadedItems;

    QQmlInstanceModel* model = nullptr;
    QVariant modelVariant;

    // loadedTable describes the table cells that are currently loaded (from top left
    // row/column to bottom right row/column). loadedTableOuterRect describes the actual
    // pixels that those cells covers, and is matched agains the viewport to determine when
    // we need to fill up with more rows/columns. loadedTableInnerRect describes the pixels
    // that the loaded table covers if you remove one row/column on each side of the table, and
    // is used to determine rows/columns that are no longer visible and can be unloaded.
    QRect loadedTable;
    QRectF loadedTableOuterRect;
    QRectF loadedTableInnerRect;

    QRectF viewportRect = QRectF(0, 0, -1, -1);
    QRectF bufferRect;

    QQmlNullableValue<int> explicitRowCount;
    QQmlNullableValue<int> explicitColumnCount;
    QSize tableSize;

    TableEdgeLoadRequest loadRequest;

    QPoint implicitSizeBenchMarkPoint = QPoint(-1, -1);
    QSizeF cellSpacing;

    int cacheBuffer = kDefaultCacheBuffer;
    QTimer cacheBufferDelayTimer;
    bool hasBufferedItems = false;

    bool blockItemCreatedCallback = false;
    bool tableInvalid = false;
    bool tableRebuilding = false;
    bool columnRowPositionsInvalid = false;

    QVector<QMetaObject::Connection> containerConnections = QVector<QMetaObject::Connection>(4);

    QVector<ColumnRowSize> columnWidths;
    QVector<ColumnRowSize> rowHeights;

    const static QPoint kLeft;
    const static QPoint kRight;
    const static QPoint kUp;
    const static QPoint kDown;

#ifdef QT_DEBUG
    QString forcedIncubationMode = qEnvironmentVariable("QT_TABLE_INCUBATION_MODE");
#endif

protected:
    inline QQmlDelegateModel *wrapperModel() { return qobject_cast<QQmlDelegateModel*>(model); }
    inline QQmlDelegateModel *wrapperModel() const { return qobject_cast<QQmlDelegateModel*>(model); }

    int modelIndexAtCell(const QPoint &cell);
    QPoint cellAtModelIndex(int modelIndex);

    void calculateColumnWidthsAfterRebuilding();
    void calculateRowHeightsAfterRebuilding();
    void calculateColumnWidth(int column);
    void calculateRowHeight(int row);
    void calculateEdgeSizeFromLoadRequest();
    void calculateTableSize();

    qreal columnWidth(int column);
    qreal rowHeight(int row);

    void relayoutTable();
    void relayoutTableItems();

    void layoutVerticalEdge(Qt::Edge tableEdge, bool adjustSize);
    void layoutHorizontalEdge(Qt::Edge tableEdge, bool adjustSize);
    void layoutTableEdgeFromLoadRequest();

    void updateImplicitWidth();
    void updateImplicitHeight();

    void enforceFirstRowColumnAtOrigo();
    void syncLoadedTableRectFromLoadedTable();
    void syncLoadedTableFromLoadRequest();

    bool canLoadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const;
    bool canUnloadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const;
    Qt::Edge nextEdgeToLoad(const QRectF rect);
    Qt::Edge nextEdgeToUnload(const QRectF rect);

    FxTableItem *loadedTableItem(const QPoint &cell) const;
    FxTableItem *itemNextTo(const FxTableItem *fxTableItem, const QPoint &direction) const;
    FxTableItem *createFxTableItem(const QPoint &cell, QQmlIncubator::IncubationMode incubationMode);
    FxTableItem *loadFxTableItem(const QPoint &cell, QQmlIncubator::IncubationMode incubationMode);

    bool releaseItem(FxTableItem *fxTableItem);
    void releaseLoadedItems();
    void clear();

    void unloadItem(const QPoint &cell);
    void unloadItems(const QLine &items);

    void loadInitialTopLeftItem();
    void loadEdgesInsideRect(const QRectF &rect, QQmlIncubator::IncubationMode incubationMode);
    void unloadEdgesOutsideRect(const QRectF &rect);
    void loadAndUnloadVisibleEdges();
    void cancelLoadRequest();
    void processLoadRequest();
    void beginRebuildTable();
    void endRebuildTable();

    void loadBuffer();
    void unloadBuffer();

    void invalidateTable();
    void invalidateColumnRowPositions();

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
    {
    }

    qreal position() const override { return 0; }
    qreal endPosition() const override { return 0; }
    qreal size() const override { return 0; }
    qreal sectionSize() const override { return 0; }
    bool contains(qreal, qreal) const override { return false; }

    QPoint cell;
};

Q_DECLARE_TYPEINFO(QQuickTablePrivate::ColumnRowSize, Q_PRIMITIVE_TYPE);

const QPoint QQuickTablePrivate::kLeft = QPoint(-1, 0);
const QPoint QQuickTablePrivate::kRight = QPoint(1, 0);
const QPoint QQuickTablePrivate::kUp = QPoint(0, -1);
const QPoint QQuickTablePrivate::kDown = QPoint(0, 1);

QQuickTablePrivate::QQuickTablePrivate()
    : QQuickItemPrivate()
{
    cacheBufferDelayTimer.setSingleShot(true);
    QObject::connect(&cacheBufferDelayTimer, &QTimer::timeout, [=]{ loadBuffer(); });
}

QString QQuickTablePrivate::tableLayoutToString() const
{
    return QString(QLatin1String("table cells: (%1,%2) -> (%3,%4), item count: %5, table rect: %6,%7 x %8,%9"))
            .arg(loadedTable.topLeft().x()).arg(loadedTable.topLeft().y())
            .arg(loadedTable.bottomRight().x()).arg(loadedTable.bottomRight().y())
            .arg(loadedItems.count())
            .arg(loadedTableOuterRect.x())
            .arg(loadedTableOuterRect.y())
            .arg(loadedTableOuterRect.width())
            .arg(loadedTableOuterRect.height());
}

void QQuickTablePrivate::dumpTable() const
{
    auto listCopy = loadedItems;
    std::stable_sort(listCopy.begin(), listCopy.end(),
        [](const FxTableItem *lhs, const FxTableItem *rhs)
        { return lhs->index < rhs->index; });

    qWarning() << QStringLiteral("******* TABLE DUMP *******");
    for (int i = 0; i < listCopy.count(); ++i)
        qWarning() << static_cast<FxTableItem *>(listCopy.at(i))->cell;
    qWarning() << tableLayoutToString();

    QString filename = QStringLiteral("QQuickTable_dumptable_capture.png");
    if (q_func()->window()->grabWindow().save(filename))
        qWarning() << "Window capture saved to:" << filename;
}

int QQuickTablePrivate::modelIndexAtCell(const QPoint &cell)
{
    // The table can have a different row/column count than the model. Whenever
    // we need to reference an item in the model, we therefore need to map the
    // cell in the table to the corresponding index in the model.
    if (auto wmodel = wrapperModel()) {
        int columnCountInModel = wmodel->columns();
        if (columnCountInModel > 1) {
            // The model has more than one column, and we therefore
            // treat it as a table. In that case, cell needs
            // to be inside the table model to have a valid index.
            int rowCountInModel = wmodel->rows();
            if (cell.x() >= columnCountInModel || cell.y() >= rowCountInModel)
                return -1;
            return cell.y() + (cell.x() * rowCountInModel);
        }
    }

    // The model has zero or one column, and we therefore treat it as a list. For
    // lists, we assign model indices to cells by filling up rows first, then columns.
    int availableRows = tableSize.height();
    int modelIndex = cell.y() + (cell.x() * availableRows);
    if (modelIndex >= model->count())
        return -1;
    return modelIndex;
}

QPoint QQuickTablePrivate::cellAtModelIndex(int modelIndex)
{
    int availableRows = tableSize.height();
    Q_TABLE_ASSERT(availableRows > 0, availableRows);
    int column = int(modelIndex / availableRows);
    int row = modelIndex % availableRows;
    return QPoint(column, row);
}

void QQuickTablePrivate::updateImplicitWidth()
{
    Q_Q(QQuickTable);

    const qreal thresholdBeforeAdjust = 0.1;
    int currentRightColumn = loadedTable.right();

    if (currentRightColumn > implicitSizeBenchMarkPoint.x()) {
        implicitSizeBenchMarkPoint.setX(currentRightColumn);

        qreal currentWidth = loadedTableOuterRect.right();
        qreal averageCellSize = currentWidth / (currentRightColumn + 1);
        qreal averageSize = averageCellSize + cellSpacing.width();
        qreal estimatedWith = (tableSize.width() * averageSize) - cellSpacing.width();

        if (currentRightColumn >= tableSize.width() - 1) {
            // We are at the last column, and can set the exact width
            if (currentWidth != q->implicitWidth())
                q->setImplicitWidth(currentWidth);
        } else if (currentWidth >= q->implicitWidth()) {
            // We are at the estimated width, but there are still more columns
            q->setImplicitWidth(estimatedWith);
        } else {
            // Only set a new width if the new estimate is substantially different
            qreal diff = 1 - (estimatedWith / q->implicitWidth());
            if (qAbs(diff) > thresholdBeforeAdjust)
                q->setImplicitWidth(estimatedWith);
        }
    }
}

void QQuickTablePrivate::updateImplicitHeight()
{
    Q_Q(QQuickTable);

    const qreal thresholdBeforeAdjust = 0.1;
    int currentBottomRow = loadedTable.bottom();

    if (currentBottomRow > implicitSizeBenchMarkPoint.y()) {
        implicitSizeBenchMarkPoint.setY(currentBottomRow);

        qreal currentHeight = loadedTableOuterRect.bottom();
        qreal averageCellSize = currentHeight / (currentBottomRow + 1);
        qreal averageSize = averageCellSize + cellSpacing.height();
        qreal estimatedHeight = (tableSize.height() * averageSize) - cellSpacing.height();

        if (currentBottomRow >= tableSize.height() - 1) {
            // We are at the last row, and can set the exact height
            if (currentHeight != q->implicitHeight())
                q->setImplicitHeight(currentHeight);
        } else if (currentHeight >= q->implicitHeight()) {
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

void QQuickTablePrivate::enforceFirstRowColumnAtOrigo()
{
    // Gaps before the first row/column can happen if rows/columns
    // changes size while flicking e.g because of spacing changes or
    // changes to a column maxWidth/row maxHeight. Check for this, and
    // move the whole table rect accordingly.
    bool layoutNeeded = false;
    const qreal flickMargin = 50;

    if (loadedTable.x() == 0 && loadedTableOuterRect.x() != 0) {
        // The table is at the beginning, but not at the edge of the
        // content view. So move the table to origo.
        loadedTableOuterRect.moveLeft(0);
        layoutNeeded = true;
    } else if (loadedTableOuterRect.x() < 0) {
        // The table is outside the beginning of the content view. Move
        // the whole table inside, and make some room for flicking.
        loadedTableOuterRect.moveLeft(loadedTable.x() == 0 ? 0 : flickMargin);
        layoutNeeded = true;
    }

    if (loadedTable.y() == 0 && loadedTableOuterRect.y() != 0) {
        loadedTableOuterRect.moveTop(0);
        layoutNeeded = true;
    } else if (loadedTableOuterRect.y() < 0) {
        loadedTableOuterRect.moveTop(loadedTable.y() == 0 ? 0 : flickMargin);
        layoutNeeded = true;
    }

    if (layoutNeeded)
        relayoutTableItems();
}

void QQuickTablePrivate::syncLoadedTableRectFromLoadedTable()
{
    QRectF topLeftRect = loadedTableItem(loadedTable.topLeft())->geometry();
    QRectF bottomRightRect = loadedTableItem(loadedTable.bottomRight())->geometry();
    loadedTableOuterRect = topLeftRect.united(bottomRightRect);
    loadedTableInnerRect = QRectF(topLeftRect.bottomRight(), bottomRightRect.topLeft());
}

void QQuickTablePrivate::syncLoadedTableFromLoadRequest()
{
    switch (loadRequest.edge()) {
    case Qt::LeftEdge:
    case Qt::TopEdge:
        loadedTable.setTopLeft(loadRequest.firstCell());
        break;
    case Qt::RightEdge:
    case Qt::BottomEdge:
        loadedTable.setBottomRight(loadRequest.lastCell());
        break;
    default:
        loadedTable = QRect(loadRequest.firstCell(), loadRequest.lastCell());
    }
}

FxTableItem *QQuickTablePrivate::itemNextTo(const FxTableItem *fxTableItem, const QPoint &direction) const
{
    return loadedTableItem(fxTableItem->cell + direction);
}

FxTableItem *QQuickTablePrivate::loadedTableItem(const QPoint &cell) const
{
    for (int i = 0; i < loadedItems.count(); ++i) {
        FxTableItem *item = loadedItems.at(i);
        if (item->cell == cell)
            return item;
    }

    Q_TABLE_UNREACHABLE(cell);
    return nullptr;
}

FxTableItem *QQuickTablePrivate::createFxTableItem(const QPoint &cell, QQmlIncubator::IncubationMode incubationMode)
{
    Q_Q(QQuickTable);

    QQuickItem *item = nullptr;
    int modelIndex = modelIndexAtCell(cell);

    if (modelIndex != -1) {
        // Requested cell is covered by the model
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
    fxTableItem->cell = cell;
    return fxTableItem;
}

FxTableItem *QQuickTablePrivate::loadFxTableItem(const QPoint &cell, QQmlIncubator::IncubationMode incubationMode)
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
    auto item = createFxTableItem(cell, incubationMode);
    qCDebug(lcTableDelegateLifecycle) << cell << "ready?" << bool(item);
    return item;
}

void QQuickTablePrivate::releaseLoadedItems() {
    // Make a copy and clear the list of items first to avoid destroyed
    // items being accessed during the loop (QTBUG-61294)
    auto const tmpList = loadedItems;
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
    tableInvalid = true;
    tableRebuilding = false;
    if (loadRequest.isActive())
        cancelLoadRequest();

    releaseLoadedItems();
    loadedTable = QRect();
    loadedTableOuterRect = QRect();
    loadedTableInnerRect = QRect();
    columnWidths.clear();
    rowHeights.clear();
    implicitSizeBenchMarkPoint = QPoint(-1, -1);

    updateImplicitWidth();
    updateImplicitHeight();
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

bool QQuickTablePrivate::canLoadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
        if (loadedTable.topLeft().x() == 0)
            return false;
        return loadedTableOuterRect.left() > fillRect.left();
    case Qt::RightEdge:
        if (loadedTable.bottomRight().x() >= tableSize.width() - 1)
            return false;
        return loadedTableOuterRect.right() < fillRect.right();
    case Qt::TopEdge:
        if (loadedTable.topLeft().y() == 0)
            return false;
        return loadedTableOuterRect.top() > fillRect.top();
    case Qt::BottomEdge:
        if (loadedTable.bottomRight().y() >= tableSize.height() - 1)
            return false;
        return loadedTableOuterRect.bottom() < fillRect.bottom();
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
        return loadedTableInnerRect.left() < fillRect.left() - floatingPointMargin;
    case Qt::RightEdge:
        if (loadedTable.width() <= 1)
            return false;
        return loadedTableInnerRect.right() > fillRect.right() + floatingPointMargin;
    case Qt::TopEdge:
        if (loadedTable.height() <= 1)
            return false;
        return loadedTableInnerRect.top() < fillRect.top() - floatingPointMargin;
    case Qt::BottomEdge:
        if (loadedTable.height() <= 1)
            return false;
        return loadedTableInnerRect.bottom() > fillRect.bottom() + floatingPointMargin;
    }
    Q_TABLE_UNREACHABLE(tableEdge);
    return false;
}

Qt::Edge QQuickTablePrivate::nextEdgeToLoad(const QRectF rect)
{
    for (Qt::Edge edge : allTableEdges) {
        if (canLoadTableEdge(edge, rect))
            return edge;
    }
    return Qt::Edge(0);
}

Qt::Edge QQuickTablePrivate::nextEdgeToUnload(const QRectF rect)
{
    for (Qt::Edge edge : allTableEdges) {
        if (canUnloadTableEdge(edge, rect))
            return edge;
    }
    return Qt::Edge(0);
}

void QQuickTablePrivate::calculateColumnWidthsAfterRebuilding()
{
    qreal prevColumnWidth = 0;
    for (int column = loadedTable.left(); column <= loadedTable.right(); ++column) {
        qreal columnWidth = 0;
        for (int row = loadedTable.top(); row <= loadedTable.bottom(); ++row) {
            auto const item = loadedTableItem(QPoint(column, row));
            columnWidth = qMax(columnWidth, item->geometry().width());
        }

        if (columnWidth == prevColumnWidth)
            continue;

        columnWidths.append({column, columnWidth});
        prevColumnWidth = columnWidth;
    }
}

void QQuickTablePrivate::calculateRowHeightsAfterRebuilding()
{
    qreal prevRowHeight = 0;
    for (int row = loadedTable.top(); row <= loadedTable.bottom(); ++row) {
        qreal rowHeight = 0;
        for (int column = loadedTable.left(); column <= loadedTable.right(); ++column) {
            auto const item = loadedTableItem(QPoint(column, row));
            rowHeight = qMax(rowHeight, item->geometry().height());
        }

        if (rowHeight == prevRowHeight)
            continue;

        rowHeights.append({row, rowHeight});
    }
}

void QQuickTablePrivate::calculateColumnWidth(int column)
{
    if (column < columnWidths.last().index) {
        // We only do the calculation once, and then stick with the size.
        // See comments inside ColumnRowSize struct.
        return;
    }

    qreal columnWidth = 0;
    for (int row = loadedTable.top(); row <= loadedTable.bottom(); ++row) {
        auto const item = loadedTableItem(QPoint(column, row));
        columnWidth = qMax(columnWidth, item->geometry().width());
    }

    if (columnWidth == columnWidths.last().size)
        return;

    columnWidths.append({column, columnWidth});
}

void QQuickTablePrivate::calculateRowHeight(int row)
{
    if (row < rowHeights.last().index) {
        // We only do the calculation once, and then stick with the size.
        // See comments inside ColumnRowSize struct.
        return;
    }

    qreal rowHeight = 0;
    for (int column = loadedTable.left(); column <= loadedTable.right(); ++column) {
        auto const item = loadedTableItem(QPoint(column, row));
        rowHeight = qMax(rowHeight, item->geometry().height());
    }

    if (rowHeight == rowHeights.last().size)
        return;

    rowHeights.append({row, rowHeight});
}

void QQuickTablePrivate::calculateEdgeSizeFromLoadRequest()
{
    if (tableRebuilding)
        return;

    switch (loadRequest.edge()) {
    case Qt::LeftEdge:
    case Qt::TopEdge:
        // Flicking left or up through "never loaded" rows/columns is currently
        // not supported. You always need to start loading the table from the beginning.
        return;
    case Qt::RightEdge:
        if (tableSize.height() > 1)
            calculateColumnWidth(loadedTable.right());
        break;
    case Qt::BottomEdge:
        if (tableSize.width() > 1)
            calculateRowHeight(loadedTable.bottom());
        break;
    default:
        Q_TABLE_UNREACHABLE("This function should not be called when loading top-left item");
    }
}

void QQuickTablePrivate::calculateTableSize()
{
    // tableSize is the same as row and column count.
    // If rows or columns is set explicit on the
    // table, we use those values. Otherwise, we use
    // whatever the model reports, depending on whether
    // it represents a list or a table.
    Q_Q(QQuickTable);
    QSize prevTableSize = tableSize;

    if (!explicitRowCount.isNull)
        tableSize.setHeight(explicitRowCount);
    else if (auto wrapper = wrapperModel())
        tableSize.setHeight(wrapper->rows());
    else if (model)
        tableSize.setHeight(model->count());
    else
        tableSize.setHeight(0);

    if (!explicitColumnCount.isNull)
        tableSize.setWidth(explicitColumnCount);
    else if (auto wrapper = wrapperModel())
        tableSize.setWidth(wrapper->columns());
    else
        tableSize.setWidth(1);

    if (prevTableSize.width() != tableSize.width())
        emit q->columnsChanged();
    if (prevTableSize.height() != tableSize.height())
        emit q->rowsChanged();
}

qreal QQuickTablePrivate::columnWidth(int column)
{
    if (!columnWidths.isEmpty()) {
        // Find the ColumnRowSize assignment before, or at, column
        auto iter = std::lower_bound(columnWidths.constBegin(), columnWidths.constEnd(),
                                     ColumnRowSize{column, -1}, ColumnRowSize::lessThan);

        // First check if we got an explicit assignment
        if (iter->index == column)
            return iter->size;

        // If the table is not a list, return the size of
        // ColumnRowSize element found before column.
        if (tableSize.height() > 1)
            return (iter - 1)->size;
    }

    // if we have an item loaded at column, return the width of the item.
    if (column >= loadedTable.left() && column <= loadedTable.right())
        return loadedTableItem(QPoint(column, 0))->geometry().width();

    return -1;
}

qreal QQuickTablePrivate::rowHeight(int row)
{
    if (!rowHeights.isEmpty()) {
        // Find the ColumnRowSize assignment before, or at, row
        auto iter = std::lower_bound(rowHeights.constBegin(), rowHeights.constEnd(),
                                     ColumnRowSize{row, -1}, ColumnRowSize::lessThan);

        // First check if we got an explicit assignment
        if (iter->index == row)
            return iter->size;

        // If the table is not a list, return the size of
        // ColumnRowSize element found before column.
        if (q_func()->columns() > 1)
            return (iter - 1)->size;
    }

    // if we have an item loaded at row, return the height of the item.
    if (row >= loadedTable.top() && row <= loadedTable.bottom())
        return loadedTableItem(QPoint(0, row))->geometry().height();

    return -1;
}

void QQuickTablePrivate::relayoutTable()
{
    relayoutTableItems();
    columnRowPositionsInvalid = false;

    syncLoadedTableRectFromLoadedTable();
    implicitSizeBenchMarkPoint = QPoint(-1, -1);
    updateImplicitWidth();
    updateImplicitHeight();
}

void QQuickTablePrivate::relayoutTableItems()
{
    qCDebug(lcTableDelegateLifecycle);
    columnRowPositionsInvalid = false;

    qreal nextColumnX = loadedTableOuterRect.x();
    qreal nextRowY = loadedTableOuterRect.y();

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

        nextColumnX += width + cellSpacing.width();
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

        nextRowY += height + cellSpacing.height();
    }

    if (Q_UNLIKELY(lcTableDelegateLifecycle().isDebugEnabled())) {
        for (int column = loadedTable.left(); column <= loadedTable.right(); ++column) {
            for (int row = loadedTable.top(); row <= loadedTable.bottom(); ++row) {
                QPoint cell = QPoint(column, row);
                qCDebug(lcTableDelegateLifecycle()) << "relayout item:" << cell << loadedTableItem(cell)->geometry();
            }
        }
    }
}

void QQuickTablePrivate::layoutVerticalEdge(Qt::Edge tableEdge, bool adjustSize)
{
    int column = (tableEdge == Qt::LeftEdge) ? loadedTable.left() : loadedTable.right();
    QPoint neighbourDirection = (tableEdge == Qt::LeftEdge) ? kRight : kLeft;
    qreal width = adjustSize ? columnWidth(column) : 0;
    qreal left = -1;

    for (int row = loadedTable.top(); row <= loadedTable.bottom(); ++row) {
        auto fxTableItem = loadedTableItem(QPoint(column, row));
        auto const neighbourItem = itemNextTo(fxTableItem, neighbourDirection);
        QRectF geometry = fxTableItem->geometry();

        if (adjustSize) {
            geometry.setWidth(width);
            geometry.setHeight(neighbourItem->geometry().height());
        }

        if (left == -1) {
            // left will be the same for all items in the
            // column, so do the calculation once.
            left = tableEdge == Qt::LeftEdge ?
                        neighbourItem->geometry().left() - cellSpacing.width() - geometry.width() :
                        neighbourItem->geometry().right() + cellSpacing.width();
        }

        geometry.moveLeft(left);
        geometry.moveTop(neighbourItem->geometry().top());

        fxTableItem->setGeometry(geometry);
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
    QPoint neighbourDirection = (tableEdge == Qt::TopEdge) ? kDown : kUp;
    qreal height = adjustSize ? rowHeight(row) : 0;
    qreal top = -1;

    for (int column = loadedTable.left(); column <= loadedTable.right(); ++column) {
        auto fxTableItem = loadedTableItem(QPoint(column, row));
        auto const neighbourItem = itemNextTo(fxTableItem, neighbourDirection);
        QRectF geometry = fxTableItem->geometry();

        if (adjustSize) {
            geometry.setWidth(neighbourItem->geometry().width());
            geometry.setHeight(height);
        }

        if (top == -1) {
            // top will be the same for all items in the
            // row, so do the calculation once.
            top = tableEdge == Qt::TopEdge ?
                neighbourItem->geometry().top() - cellSpacing.height() - geometry.height() :
                neighbourItem->geometry().bottom() + cellSpacing.height();
        }

        geometry.moveTop(top);
        geometry.moveLeft(neighbourItem->geometry().left());

        fxTableItem->setGeometry(geometry);
        fxTableItem->setVisible(true);

        qCDebug(lcTableDelegateLifecycle()) << "layout item:"
                                            << QPoint(column, row)
                                            << fxTableItem->geometry()
                                            << "adjust size:" << adjustSize;
    }
}

void QQuickTablePrivate::layoutTableEdgeFromLoadRequest()
{
    // If tableRebuilding is true, we avoid adjusting cell sizes until all items
    // needed to fill up the viewport has been loaded. This way we can base the later
    // relayoutTable() on the original size of the items. But we still need to
    // position the items now at approximate positions so we know (roughly) how many
    // rows/columns we need to load before we can consider the viewport as filled up.
    // Only then will the table rebuilding be considered done, and relayoutTable() called.
    // The relayout might cause new rows and columns to be loaded/unloaded depending on
    // whether the new sizes reveals or hides already loaded rows/columns.
    const bool adjustSize = !tableRebuilding;

    switch (loadRequest.edge()) {
    case Qt::LeftEdge:
    case Qt::RightEdge:
        layoutVerticalEdge(loadRequest.edge(), adjustSize);
        break;
    case Qt::TopEdge:
    case Qt::BottomEdge:
        layoutHorizontalEdge(loadRequest.edge(), adjustSize);
        break;
    default:
        // Request loaded top-left item rather than an edge.
        // ###todo: support starting with other top-left items than 0,0
        Q_TABLE_ASSERT(loadRequest.firstCell() == QPoint(0, 0), loadRequest.toString());
        loadedTableItem(QPoint(0, 0))->setVisible(true);
        qCDebug(lcTableDelegateLifecycle) << "top-left item geometry:" << loadedTableItem(QPoint(0, 0))->geometry();
        break;
    }
}

void QQuickTablePrivate::cancelLoadRequest()
{
    loadRequest.markAsDone();
    model->cancel(modelIndexAtCell(loadRequest.currentCell()));

    if (tableInvalid) {
        // No reason to rollback already loaded edge items
        // since we anyway are about to reload all items.
        return;
    }

    if (loadRequest.atBeginning()) {
        // No items have yet been loaded, so nothing to unload
        return;
    }

    QLine rollbackItems;
    rollbackItems.setP1(loadRequest.firstCell());
    rollbackItems.setP2(loadRequest.previousCell());
    qCDebug(lcTableDelegateLifecycle()) << "rollback:" << rollbackItems << tableLayoutToString();
    unloadItems(rollbackItems);
}

void QQuickTablePrivate::processLoadRequest()
{
    Q_TABLE_ASSERT(loadRequest.isActive(), "");

    while (loadRequest.hasCurrentCell()) {
        QPoint cell = loadRequest.currentCell();
        FxTableItem *fxTableItem = loadFxTableItem(cell, loadRequest.incubationMode());

        if (!fxTableItem) {
            // Requested item is not yet ready. Just leave, and wait for this
            // function to be called again when the item is ready.
            return;
        }

        loadedItems.append(fxTableItem);
        loadRequest.moveToNextCell();
    }

    qCDebug(lcTableDelegateLifecycle()) << "all items loaded!";

    syncLoadedTableFromLoadRequest();
    calculateEdgeSizeFromLoadRequest();
    layoutTableEdgeFromLoadRequest();

    syncLoadedTableRectFromLoadedTable();
    enforceFirstRowColumnAtOrigo();
    updateImplicitWidth();
    updateImplicitHeight();

    loadRequest.markAsDone();
    qCDebug(lcTableDelegateLifecycle()) << "request completed! Table:" << tableLayoutToString();
}

void QQuickTablePrivate::beginRebuildTable()
{
    qCDebug(lcTableDelegateLifecycle());
    clear();
    tableInvalid = false;
    tableRebuilding = true;
    calculateTableSize();
    loadInitialTopLeftItem();
    loadAndUnloadVisibleEdges();
}

void QQuickTablePrivate::endRebuildTable()
{
    tableRebuilding = false;

    if (loadedItems.isEmpty())
        return;

    // We don't calculate row/column sizes for lists.
    // Instead we we use the sizes of the items directly
    // unless for explicit row/column size assignments.
    columnWidths.clear();
    rowHeights.clear();
    if (tableSize.height() > 1)
        calculateColumnWidthsAfterRebuilding();
    if (tableSize.width() > 1)
        calculateRowHeightsAfterRebuilding();

    relayoutTable();
    qCDebug(lcTableDelegateLifecycle()) << tableLayoutToString();
}

void QQuickTablePrivate::loadInitialTopLeftItem()
{
    Q_TABLE_ASSERT(loadedItems.isEmpty(), "");

    if (tableSize.isEmpty())
        return;

    // Load top-left item. After loaded, loadItemsInsideRect() will take
    // care of filling out the rest of the table.
    loadRequest.begin(QPoint(0, 0), QQmlIncubator::AsynchronousIfNested);
    processLoadRequest();
}

void QQuickTablePrivate::unloadEdgesOutsideRect(const QRectF &rect)
{
    while (Qt::Edge edge = nextEdgeToUnload(rect)) {
        unloadItems(rectangleEdge(loadedTable, edge));
        loadedTable = expandedRect(loadedTable, edge, -1);
        syncLoadedTableRectFromLoadedTable();
        qCDebug(lcTableDelegateLifecycle) << tableLayoutToString();
    }
}

void QQuickTablePrivate::loadEdgesInsideRect(const QRectF &rect, QQmlIncubator::IncubationMode incubationMode)
{
    while (Qt::Edge edge = nextEdgeToLoad(rect)) {
        QLine cellsToLoad = rectangleEdge(expandedRect(loadedTable, edge, 1), edge);
        loadRequest.begin(cellsToLoad, edge, incubationMode);
        processLoadRequest();
        if (loadRequest.isActive())
            return;
    }
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

    if (loadRequest.isActive()) {
        // Don't start loading more edges while we're
        // already waiting for another one to load.
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
    if (cacheBuffer <= 0 || loadRequest.isActive() || loadedItems.isEmpty())
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
    cacheBufferDelayTimer.stop();
    if (loadRequest.isActive())
        cancelLoadRequest();
    unloadEdgesOutsideRect(viewportRect);
}

void QQuickTablePrivate::invalidateTable() {
    tableInvalid = true;
    if (loadRequest.isActive())
        cancelLoadRequest();
    q_func()->polish();
}

void QQuickTablePrivate::invalidateColumnRowPositions() {
    columnRowPositionsInvalid = true;
    q_func()->polish();
}

void QQuickTablePrivate::updatePolish()
{
    if (loadRequest.isActive()) {
        // We're loading items async, so don't continue until all requested items have been
        // received. Note that updatePolish is called once the loadRequest has completed.
        return;
    }

    if (!viewportRect.isValid())
        return;

    bufferRect = viewportRect.adjusted(-cacheBuffer, -cacheBuffer, cacheBuffer, cacheBuffer);

    if (tableInvalid) {
        beginRebuildTable();
        if (loadRequest.isActive())
            return;
    }

    if (tableRebuilding)
        endRebuildTable();

    if (loadedItems.isEmpty()) {
        qCDebug(lcTableDelegateLifecycle()) << "no items loaded, meaning empty model or row/column == 0";
        return;
    }

    if (columnRowPositionsInvalid)
        relayoutTable();

    if (hasBufferedItems && nextEdgeToLoad(viewportRect)) {
        // We are going to load more edges, so trim down the table as much
        // as possible to avoid loading cells that are outside the viewport.
        unloadBuffer();
    }

    loadAndUnloadVisibleEdges();
    if (loadRequest.isActive())
        return;

    if (cacheBuffer > 0) {
        // When polish hasn't been called for a while (which means that the viewport
        // rect hasn't changed), we start buffering items. We delay this operation by
        // using a timer to increase performance (by not loading hidden items) while
        // the user is flicking.
        cacheBufferDelayTimer.start(kBufferTimerInterval);
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
    return d_func()->tableSize.height();
}

void QQuickTable::setRows(int rows)
{
    Q_D(QQuickTable);
    if ((!d->explicitRowCount.isNull && d->explicitRowCount == rows) || rows < 0)
        return;

    d->explicitRowCount = rows;
    d->invalidateTable();
    emit rowsChanged();
}

void QQuickTable::resetRows()
{
    Q_D(QQuickTable);
    if (d->explicitRowCount.isNull)
        return;

    d->explicitRowCount = QQmlNullableValue<int>();
    d->invalidateTable();
    emit rowsChanged();
}

int QQuickTable::columns() const
{
    return d_func()->tableSize.width();
}

void QQuickTable::setColumns(int columns)
{
    Q_D(QQuickTable);
    if ((!d->explicitColumnCount.isNull && d->explicitColumnCount == columns) || columns < 0)
        return;

    d->explicitColumnCount = columns;
    d->invalidateTable();
    emit columnsChanged();
}

void QQuickTable::resetColumns()
{
    Q_D(QQuickTable);
    if (d->explicitColumnCount.isNull)
        return;

    d->explicitColumnCount = QQmlNullableValue<int>();
    d->invalidateTable();
    emit columnsChanged();
}

qreal QQuickTable::rowSpacing() const
{
    return d_func()->cellSpacing.height();
}

void QQuickTable::setRowSpacing(qreal spacing)
{
    Q_D(QQuickTable);
    if (qFuzzyCompare(d->cellSpacing.height(), spacing))
        return;

    d->cellSpacing.setHeight(spacing);
    d->invalidateColumnRowPositions();
    emit rowSpacingChanged();
}

qreal QQuickTable::columnSpacing() const
{
    return d_func()->cellSpacing.width();
}

void QQuickTable::setColumnSpacing(qreal spacing)
{
    Q_D(QQuickTable);
    if (qFuzzyCompare(d->cellSpacing.width(), spacing))
        return;

    d->cellSpacing.setWidth(spacing);
    d->invalidateColumnRowPositions();
    emit columnSpacingChanged();
}

int QQuickTable::cacheBuffer() const
{
    return d_func()->cacheBuffer;
}

void QQuickTable::setCacheBuffer(int newBuffer)
{
    Q_D(QQuickTable);
    if (d->cacheBuffer == newBuffer || newBuffer < 0)
        return;

    d->cacheBuffer = newBuffer;

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

    qCDebug(lcTableDelegateLifecycle) << "item done loading:"
        << d->cellAtModelIndex(modelIndex);

    // Since the item we waited for has finished incubating, we can
    // continue with the load request. processLoadRequest will
    // ask the model for the requested item once more, which will be
    // quick since the model has cached it.
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

    QObject *attachedObject = qmlAttachedPropertiesObject<QQuickTable>(item);
    if (QQuickTableAttached *attached = static_cast<QQuickTableAttached *>(attachedObject)) {
        attached->setTable(this);
        // Even though row and column is injected directly into the context of a delegate item
        // from QQmlDelegateModel and its model classes, they will only return which row and
        // column an item represents in the model. This might be different from which
        // cell an item ends up in in the Table, if a different rows/columns has been set
        // on it (which is typically the case for list models). For those cases, Table.row
        // and Table.column can be helpful.
        QPoint cell = d_func()->cellAtModelIndex(modelIndex);
        attached->setColumn(cell.x());
        attached->setRow(cell.y());
    }
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
