﻿/****************************************************************************
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
#include "qquicktableview_p_p.h"

#include <QtCore/qtimer.h>
#include <QtCore/qdir.h>
#include <QtQml/private/qqmldelegatemodel_p.h>
#include <QtQml/private/qqmldelegatemodel_p_p.h>
#include <QtQml/private/qqmlincubator_p.h>
#include <QtQml/private/qqmlchangeset_p.h>
#include <QtQml/qqmlinfo.h>

#include <QtQuick/private/qquickflickable_p_p.h>
#include <QtQuick/private/qquickitemviewfxitem_p_p.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcTableViewDelegateLifecycle, "qt.quick.tableview.lifecycle")

#define Q_TABLEVIEW_UNREACHABLE(output) { dumpTable(); qWarning() << "output:" << output; Q_UNREACHABLE(); }
#define Q_TABLEVIEW_ASSERT(cond, output) Q_ASSERT((cond) || [&](){ dumpTable(); qWarning() << "output:" << output; return false;}())

static const Qt::Edge allTableEdges[] = { Qt::LeftEdge, Qt::RightEdge, Qt::TopEdge, Qt::BottomEdge };
static const int kBufferTimerInterval = 300;

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

const QPoint QQuickTableViewPrivate::kLeft = QPoint(-1, 0);
const QPoint QQuickTableViewPrivate::kRight = QPoint(1, 0);
const QPoint QQuickTableViewPrivate::kUp = QPoint(0, -1);
const QPoint QQuickTableViewPrivate::kDown = QPoint(0, 1);

QQuickTableViewPrivate::QQuickTableViewPrivate()
    : QQuickFlickablePrivate()
{
    cacheBufferDelayTimer.setSingleShot(true);
    QObject::connect(&cacheBufferDelayTimer, &QTimer::timeout, [=]{ loadBuffer(); });
}

QQuickTableViewPrivate::~QQuickTableViewPrivate()
{
    releaseLoadedItems();
    if (tableModel)
        delete tableModel;
}

QString QQuickTableViewPrivate::tableLayoutToString() const
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

void QQuickTableViewPrivate::dumpTable() const
{
    auto listCopy = loadedItems.values();
    std::stable_sort(listCopy.begin(), listCopy.end(),
        [](const FxTableItem *lhs, const FxTableItem *rhs)
        { return lhs->index < rhs->index; });

    qWarning() << QStringLiteral("******* TABLE DUMP *******");
    for (int i = 0; i < listCopy.count(); ++i)
        qWarning() << static_cast<FxTableItem *>(listCopy.at(i))->cell;
    qWarning() << tableLayoutToString();

    const QString filename = QStringLiteral("QQuickTableView_dumptable_capture.png");
    const QString path = QDir::current().absoluteFilePath(filename);
    if (q_func()->window() && q_func()->window()->grabWindow().save(path))
        qWarning() << "Window capture saved to:" << path;
}

QQuickTableViewAttached *QQuickTableViewPrivate::getAttachedObject(const QObject *object) const
{
    QObject *attachedObject = qmlAttachedPropertiesObject<QQuickTableView>(object);
    return static_cast<QQuickTableViewAttached *>(attachedObject);
}

int QQuickTableViewPrivate::modelIndexAtCell(const QPoint &cell) const
{
    int availableRows = tableSize.height();
    int modelIndex = cell.y() + (cell.x() * availableRows);
    Q_TABLEVIEW_ASSERT(modelIndex < model->count(),
        "modelIndex:" << modelIndex << "cell:" << cell << "count:" << model->count());
    return modelIndex;
}

QPoint QQuickTableViewPrivate::cellAtModelIndex(int modelIndex) const
{
    int availableRows = tableSize.height();
    Q_TABLEVIEW_ASSERT(availableRows > 0, availableRows);
    int column = int(modelIndex / availableRows);
    int row = modelIndex % availableRows;
    return QPoint(column, row);
}

void QQuickTableViewPrivate::updateContentWidth()
{
    Q_Q(QQuickTableView);

    if (explicitContentWidth.isValid()) {
        // Don't calculate contentWidth when it
        // was set explicitly by the application.
        return;
    }

    const qreal thresholdBeforeAdjust = 0.1;
    int currentRightColumn = loadedTable.right();

    if (currentRightColumn > contentSizeBenchMarkPoint.x()) {
        contentSizeBenchMarkPoint.setX(currentRightColumn);

        const qreal spacing = currentRightColumn * cellSpacing.width();
        const qreal margins = tableMargins.left() + tableMargins.right();
        qreal currentWidth = loadedTableOuterRect.right() - tableMargins.left();
        const qreal averageCellWidth = (currentWidth - spacing) / (currentRightColumn + 1);
        qreal estimatedWidth = (tableSize.width() * (averageCellWidth + cellSpacing.width())) - cellSpacing.width();

        currentWidth += margins;
        estimatedWidth += margins;

        if (currentRightColumn >= tableSize.width() - 1) {
            // We are at the last column, and can set the exact width
            if (currentWidth != q->implicitWidth())
                q->setContentWidth(currentWidth);
        } else if (currentWidth >= q->implicitWidth()) {
            // We are at the estimated width, but there are still more columns
            q->setContentWidth(estimatedWidth);
        } else {
            // Only set a new width if the new estimate is substantially different
            qreal diff = 1 - (estimatedWidth / q->implicitWidth());
            if (qAbs(diff) > thresholdBeforeAdjust)
                q->setContentWidth(estimatedWidth);
        }
    }
}

void QQuickTableViewPrivate::updateContentHeight()
{
    Q_Q(QQuickTableView);

    if (explicitContentHeight.isValid()) {
        // Don't calculate contentHeight when it
        // was set explicitly by the application.
        return;
    }

    const qreal thresholdBeforeAdjust = 0.1;
    int currentBottomRow = loadedTable.bottom();

    if (currentBottomRow > contentSizeBenchMarkPoint.y()) {
        contentSizeBenchMarkPoint.setY(currentBottomRow);

        const qreal spacing = currentBottomRow * cellSpacing.height();
        const qreal margins = tableMargins.top() + tableMargins.bottom();
        qreal currentHeight = loadedTableOuterRect.bottom() - tableMargins.top();
        const qreal averageCellHeight = (currentHeight - spacing) / (currentBottomRow + 1);
        qreal estimatedHeight = (tableSize.height() * (averageCellHeight + cellSpacing.height())) - cellSpacing.height();

        currentHeight += margins;
        estimatedHeight += margins;

        if (currentBottomRow >= tableSize.height() - 1) {
            // We are at the last row, and can set the exact height
            if (currentHeight != q->implicitHeight())
                q->setContentHeight(currentHeight);
        } else if (currentHeight >= q->implicitHeight()) {
            // We are at the estimated height, but there are still more rows
            q->setContentHeight(estimatedHeight);
        } else {
            // Only set a new height if the new estimate is substantially different
            qreal diff = 1 - (estimatedHeight / q->implicitHeight());
            if (qAbs(diff) > thresholdBeforeAdjust)
                q->setContentHeight(estimatedHeight);
        }
    }
}

void QQuickTableViewPrivate::enforceTableAtOrigin()
{
    // Gaps before the first row/column can happen if rows/columns
    // changes size while flicking e.g because of spacing changes or
    // changes to a column maxWidth/row maxHeight. Check for this, and
    // move the whole table rect accordingly.
    bool layoutNeeded = false;
    const qreal flickMargin = 50;

    if (loadedTable.x() == 0 && loadedTableOuterRect.x() != tableMargins.left()) {
        // The table is at the beginning, but not at the edge of the
        // content view. So move the table to origo.
        loadedTableOuterRect.moveLeft(tableMargins.left());
        layoutNeeded = true;
    } else if (loadedTableOuterRect.x() < 0) {
        // The table is outside the beginning of the content view. Move
        // the whole table inside, and make some room for flicking.
        loadedTableOuterRect.moveLeft(tableMargins.left() + loadedTable.x() == 0 ? 0 : flickMargin);
        layoutNeeded = true;
    }

    if (loadedTable.y() == 0 && loadedTableOuterRect.y() != tableMargins.top()) {
        loadedTableOuterRect.moveTop(tableMargins.top());
        layoutNeeded = true;
    } else if (loadedTableOuterRect.y() < 0) {
        loadedTableOuterRect.moveTop(tableMargins.top() + loadedTable.y() == 0 ? 0 : flickMargin);
        layoutNeeded = true;
    }

    if (layoutNeeded)
        relayoutTableItems();
}

void QQuickTableViewPrivate::syncLoadedTableRectFromLoadedTable()
{
    QRectF topLeftRect = loadedTableItem(loadedTable.topLeft())->geometry();
    QRectF bottomRightRect = loadedTableItem(loadedTable.bottomRight())->geometry();
    loadedTableOuterRect = topLeftRect.united(bottomRightRect);
    loadedTableInnerRect = QRectF(topLeftRect.bottomRight(), bottomRightRect.topLeft());
}

void QQuickTableViewPrivate::syncLoadedTableFromLoadRequest()
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

FxTableItem *QQuickTableViewPrivate::itemNextTo(const FxTableItem *fxTableItem, const QPoint &direction) const
{
    return loadedTableItem(fxTableItem->cell + direction);
}

FxTableItem *QQuickTableViewPrivate::loadedTableItem(const QPoint &cell) const
{
    const int modelIndex = modelIndexAtCell(cell);
    Q_TABLEVIEW_ASSERT(loadedItems.contains(modelIndex), modelIndex << cell);
    return loadedItems.value(modelIndex);
}

FxTableItem *QQuickTableViewPrivate::createFxTableItem(const QPoint &cell, QQmlIncubator::IncubationMode incubationMode)
{
    Q_Q(QQuickTableView);

    bool ownItem = false;
    int modelIndex = modelIndexAtCell(cell);

    QObject* object = model->object(modelIndex, incubationMode);
    if (!object) {
        if (model->incubationStatus(modelIndex) == QQmlIncubator::Loading) {
            // Item is incubating. Return nullptr for now, and let the table call this
            // function again once we get a callback to itemCreatedCallback().
            return nullptr;
        }

        qWarning() << "TableView: failed loading index:" << modelIndex;
        object = new QQuickItem();
        ownItem = true;
    }

    QQuickItem *item = qmlobject_cast<QQuickItem*>(object);
    if (!item) {
        // The model could not provide an QQuickItem for the
        // given index, so we create a placeholder instead.
        qWarning() << "TableView: delegate is not an item:" << modelIndex;
        model->release(object);
        item = new QQuickItem();
        ownItem = true;
    }

    if (ownItem) {
        // Parent item is normally set early on from initItemCallback (to
        // allow bindings to the parent property). But if we created the item
        // within this function, we need to set it explicit.
        item->setParentItem(q->contentItem());
    }
    Q_TABLEVIEW_ASSERT(item->parentItem() == q->contentItem(), item->parentItem());

    FxTableItem *fxTableItem = new FxTableItem(item, q, ownItem);
    fxTableItem->setVisible(false);
    fxTableItem->cell = cell;
    fxTableItem->index = modelIndex;
    return fxTableItem;
}

FxTableItem *QQuickTableViewPrivate::loadFxTableItem(const QPoint &cell, QQmlIncubator::IncubationMode incubationMode)
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
    qCDebug(lcTableViewDelegateLifecycle) << cell << "ready?" << bool(item);
    return item;
}

void QQuickTableViewPrivate::releaseLoadedItems() {
    // Make a copy and clear the list of items first to avoid destroyed
    // items being accessed during the loop (QTBUG-61294)
    auto const tmpList = loadedItems;
    loadedItems.clear();
    for (FxTableItem *item : tmpList)
        releaseItem(item, QQmlTableInstanceModel::NotReusable);
}

void QQuickTableViewPrivate::releaseItem(FxTableItem *fxTableItem, QQmlTableInstanceModel::ReusableFlag reusableFlag)
{
    Q_TABLEVIEW_ASSERT(fxTableItem->item, fxTableItem->index);

    if (fxTableItem->ownItem) {
        delete fxTableItem->item;
    } else {
        // Only QQmlTableInstanceModel supports reusing items
        auto releaseFlag = tableModel ?
                    tableModel->release(fxTableItem->item, reusableFlag) :
                    model->release(fxTableItem->item);

        if (releaseFlag != QQmlInstanceModel::Destroyed) {
            // When items are not released, it typically means that the item is reused, or
            // that the model is an ObjectModel. If so, we just hide the item instead.
            fxTableItem->setVisible(false);
        }
    }

    delete fxTableItem;
}

void QQuickTableViewPrivate::unloadItem(const QPoint &cell)
{
    const int modelIndex = modelIndexAtCell(cell);
    Q_TABLEVIEW_ASSERT(loadedItems.contains(modelIndex), modelIndex << cell);
    releaseItem(loadedItems.take(modelIndex), reusableFlag);
}

void QQuickTableViewPrivate::unloadItems(const QLine &items)
{
    qCDebug(lcTableViewDelegateLifecycle) << items;

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

bool QQuickTableViewPrivate::canLoadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const
{
    switch (tableEdge) {
    case Qt::LeftEdge:
        if (loadedTable.topLeft().x() == 0)
            return false;
        return loadedTableOuterRect.left() > fillRect.left() + cellSpacing.width();
    case Qt::RightEdge:
        if (loadedTable.bottomRight().x() >= tableSize.width() - 1)
            return false;
        return loadedTableOuterRect.right() < fillRect.right() - cellSpacing.width();
    case Qt::TopEdge:
        if (loadedTable.topLeft().y() == 0)
            return false;
        return loadedTableOuterRect.top() > fillRect.top() + cellSpacing.height();
    case Qt::BottomEdge:
        if (loadedTable.bottomRight().y() >= tableSize.height() - 1)
            return false;
        return loadedTableOuterRect.bottom() < fillRect.bottom() - cellSpacing.height();
    }

    return false;
}

bool QQuickTableViewPrivate::canUnloadTableEdge(Qt::Edge tableEdge, const QRectF fillRect) const
{
    // Note: if there is only one row or column left, we cannot unload, since
    // they are needed as anchor point for further layouting.
    switch (tableEdge) {
    case Qt::LeftEdge:
        if (loadedTable.width() <= 1)
            return false;
        return loadedTableInnerRect.left() <= fillRect.left();
    case Qt::RightEdge:
        if (loadedTable.width() <= 1)
            return false;
        return loadedTableInnerRect.right() >= fillRect.right();
    case Qt::TopEdge:
        if (loadedTable.height() <= 1)
            return false;
        return loadedTableInnerRect.top() <= fillRect.top();
    case Qt::BottomEdge:
        if (loadedTable.height() <= 1)
            return false;
        return loadedTableInnerRect.bottom() >= fillRect.bottom();
    }
    Q_TABLEVIEW_UNREACHABLE(tableEdge);
    return false;
}

Qt::Edge QQuickTableViewPrivate::nextEdgeToLoad(const QRectF rect)
{
    for (Qt::Edge edge : allTableEdges) {
        if (canLoadTableEdge(edge, rect))
            return edge;
    }
    return Qt::Edge(0);
}

Qt::Edge QQuickTableViewPrivate::nextEdgeToUnload(const QRectF rect)
{
    for (Qt::Edge edge : allTableEdges) {
        if (canUnloadTableEdge(edge, rect))
            return edge;
    }
    return Qt::Edge(0);
}

qreal QQuickTableViewPrivate::cellWidth(const QPoint& cell)
{
    // Using an items width directly is not an option, since we change
    // it during layout (which would also cause problems when recycling items).
    auto const cellItem = loadedTableItem(cell)->item;
    return cellItem->implicitWidth();
}

qreal QQuickTableViewPrivate::cellHeight(const QPoint& cell)
{
    // Using an items height directly is not an option, since we change
    // it during layout (which would also cause problems when recycling items).
    auto const cellItem = loadedTableItem(cell)->item;
    return cellItem->implicitHeight();
}

qreal QQuickTableViewPrivate::sizeHintForColumn(int column)
{
    // Find the widest cell in the column, and return its width
    qreal columnWidth = 0;
    for (int row = loadedTable.top(); row <= loadedTable.bottom(); ++row)
        columnWidth = qMax(columnWidth, cellWidth(QPoint(column, row)));

    return columnWidth;
}

qreal QQuickTableViewPrivate::sizeHintForRow(int row)
{
    // Find the highest cell in the row, and return its height
    qreal rowHeight = 0;
    for (int column = loadedTable.left(); column <= loadedTable.right(); ++column)
        rowHeight = qMax(rowHeight, cellHeight(QPoint(column, row)));

    return rowHeight;
}

void QQuickTableViewPrivate::calculateTableSize()
{
    // tableSize is the same as row and column count, and will always
    // be the same as the number of rows and columns in the model.
    Q_Q(QQuickTableView);
    QSize prevTableSize = tableSize;

    if (tableModel)
        tableSize = QSize(tableModel->columns(), tableModel->rows());
    else if (model)
        tableSize = QSize(1, model->count());
    else
        tableSize = QSize(0, 0);

    if (prevTableSize.width() != tableSize.width())
        emit q->columnsChanged();
    if (prevTableSize.height() != tableSize.height())
        emit q->rowsChanged();
}

qreal QQuickTableViewPrivate::resolveColumnWidth(int column)
{
    Q_TABLEVIEW_ASSERT(column >= loadedTable.left() && column <= loadedTable.right(), column);
    qreal columnWidth = -1;

    if (!columnWidthProvider.isUndefined()) {
        if (columnWidthProvider.isCallable()) {
            auto const columnAsArgument = QJSValueList() << QJSValue(column);
            columnWidth = columnWidthProvider.call(columnAsArgument).toNumber();
            if (qIsNaN(columnWidth) || columnWidth <= 0) {
                // The column width needs to be greater than 0, otherwise we never reach the edge
                // while loading/refilling columns. This would cause the application to hang.
                if (!layoutWarningIssued) {
                    layoutWarningIssued = true;
                    qmlWarning(q_func()) << "columnWidthProvider did not return a valid width for column: " << column;
                }
                columnWidth = kDefaultColumnWidth;
            }
        } else {
            if (!layoutWarningIssued) {
                layoutWarningIssued = true;
                qmlWarning(q_func()) << "columnWidthProvider doesn't contain a function";
            }
            columnWidth = kDefaultColumnWidth;
        }
    } else {
        // If columnWidthProvider is left unspecified, we just iterate over the currently visible items in
        // the column. The downside of doing that, is that the column width will then only be based
        // on the implicit width of the currently loaded items (which can be different depending on
        // which row you're at when the column is flicked in). The upshot is that you don't have to
        // bother setting columnWidthProvider for small tables, or if the implicit width doesn't vary.
        columnWidth = sizeHintForColumn(column);
        if (qIsNaN(columnWidth) || columnWidth <= 0) {
            // The column width needs to be greater than 0, otherwise we never reach the edge
            // while loading/refilling columns. This would cause the application to hang.
            if (!layoutWarningIssued) {
                layoutWarningIssued = true;
                qmlWarning(q_func()) << "the delegate's implicitWidth needs to be greater than zero";
            }
            columnWidth = kDefaultColumnWidth;
        }
    }

    return columnWidth;
}

qreal QQuickTableViewPrivate::resolveRowHeight(int row)
{
    Q_TABLEVIEW_ASSERT(row >= loadedTable.top() && row <= loadedTable.bottom(), row);
    qreal rowHeight = -1;

    if (!rowHeightProvider.isUndefined()) {
        if (rowHeightProvider.isCallable()) {
            auto const rowAsArgument = QJSValueList() << QJSValue(row);
            rowHeight = rowHeightProvider.call(rowAsArgument).toNumber();
            if (qIsNaN(rowHeight) || rowHeight <= 0) {
                // The row height needs to be greater than 0, otherwise we never reach the edge
                // while loading/refilling rows. This would cause the application to hang.
                if (!layoutWarningIssued) {
                    layoutWarningIssued = true;
                    qmlWarning(q_func()) << "rowHeightProvider did not return a valid height for row: " << row;
                }
                rowHeight = kDefaultRowHeight;
            }
        } else {
            if (!layoutWarningIssued) {
                layoutWarningIssued = true;
                qmlWarning(q_func()) << "rowHeightProvider doesn't contain a function";
            }
            rowHeight = kDefaultRowHeight;
        }
    } else {
        // If rowHeightProvider is left unspecified, we just iterate over the currently visible items in
        // the row. The downside of doing that, is that the row height will then only be based
        // on the implicit height of the currently loaded items (which can be different depending on
        // which column you're at when the row is flicked in). The upshot is that you don't have to
        // bother setting rowHeightProvider for small tables, or if the implicit height doesn't vary.
        rowHeight = sizeHintForRow(row);
        if (qIsNaN(rowHeight) || rowHeight <= 0) {
            if (!layoutWarningIssued) {
                layoutWarningIssued = true;
                qmlWarning(q_func()) << "the delegate's implicitHeight needs to be greater than zero";
            }
            rowHeight = kDefaultRowHeight;
        }
    }

    return rowHeight;
}

void QQuickTableViewPrivate::relayoutTable()
{
    relayoutTableItems();
    syncLoadedTableRectFromLoadedTable();
    enforceTableAtOrigin();
    contentSizeBenchMarkPoint = QPoint(-1, -1);
    updateContentWidth();
    updateContentHeight();
    // Return back to updatePolish to loadAndUnloadVisibleEdges()
    // since the re-layout might have caused some edges to be pushed
    // out, while others might have been pushed in.
}

void QQuickTableViewPrivate::relayoutTableItems()
{
    qCDebug(lcTableViewDelegateLifecycle);
    columnRowPositionsInvalid = false;

    qreal nextColumnX = loadedTableOuterRect.x();
    qreal nextRowY = loadedTableOuterRect.y();

    for (int column = loadedTable.left(); column <= loadedTable.right(); ++column) {
        // Adjust the geometry of all cells in the current column
        const qreal width = resolveColumnWidth(column);

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
        const qreal height = resolveRowHeight(row);

        for (int column = loadedTable.left(); column <= loadedTable.right(); ++column) {
            auto item = loadedTableItem(QPoint(column, row));
            QRectF geometry = item->geometry();
            geometry.moveTop(nextRowY);
            geometry.setHeight(height);
            item->setGeometry(geometry);
        }

        nextRowY += height + cellSpacing.height();
    }

    if (Q_UNLIKELY(lcTableViewDelegateLifecycle().isDebugEnabled())) {
        for (int column = loadedTable.left(); column <= loadedTable.right(); ++column) {
            for (int row = loadedTable.top(); row <= loadedTable.bottom(); ++row) {
                QPoint cell = QPoint(column, row);
                qCDebug(lcTableViewDelegateLifecycle()) << "relayout item:" << cell << loadedTableItem(cell)->geometry();
            }
        }
    }
}

void QQuickTableViewPrivate::layoutVerticalEdge(Qt::Edge tableEdge)
{
    int column = (tableEdge == Qt::LeftEdge) ? loadedTable.left() : loadedTable.right();
    QPoint neighbourDirection = (tableEdge == Qt::LeftEdge) ? kRight : kLeft;
    qreal width = resolveColumnWidth(column);
    qreal left = -1;

    for (int row = loadedTable.top(); row <= loadedTable.bottom(); ++row) {
        auto fxTableItem = loadedTableItem(QPoint(column, row));
        auto const neighbourItem = itemNextTo(fxTableItem, neighbourDirection);

        QRectF geometry = fxTableItem->geometry();
        geometry.setWidth(width);
        geometry.setHeight(neighbourItem->geometry().height());

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

        qCDebug(lcTableViewDelegateLifecycle()) << "layout item:" << QPoint(column, row) << fxTableItem->geometry();
    }
}

void QQuickTableViewPrivate::layoutHorizontalEdge(Qt::Edge tableEdge)
{
    int row = (tableEdge == Qt::TopEdge) ? loadedTable.top() : loadedTable.bottom();
    QPoint neighbourDirection = (tableEdge == Qt::TopEdge) ? kDown : kUp;
    qreal height = resolveRowHeight(row);
    qreal top = -1;

    for (int column = loadedTable.left(); column <= loadedTable.right(); ++column) {
        auto fxTableItem = loadedTableItem(QPoint(column, row));
        auto const neighbourItem = itemNextTo(fxTableItem, neighbourDirection);

        QRectF geometry = fxTableItem->geometry();
        geometry.setWidth(neighbourItem->geometry().width());
        geometry.setHeight(height);

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

        qCDebug(lcTableViewDelegateLifecycle()) << "layout item:" << QPoint(column, row) << fxTableItem->geometry();
    }
}

void QQuickTableViewPrivate::layoutTopLeftItem()
{
    // ###todo: support starting with other top-left items than 0,0
    const QPoint cell = loadRequest.firstCell();
    Q_TABLEVIEW_ASSERT(cell == QPoint(0, 0), loadRequest.toString());
    auto topLeftItem = loadedTableItem(cell);
    auto item = topLeftItem->item;

    item->setPosition(QPoint(tableMargins.left(), tableMargins.top()));
    item->setSize(QSizeF(resolveColumnWidth(cell.x()), resolveRowHeight(cell.y())));
    topLeftItem->setVisible(true);
    qCDebug(lcTableViewDelegateLifecycle) << "geometry:" << topLeftItem->geometry();
}

void QQuickTableViewPrivate::layoutTableEdgeFromLoadRequest()
{
    switch (loadRequest.edge()) {
    case Qt::LeftEdge:
    case Qt::RightEdge:
        layoutVerticalEdge(loadRequest.edge());
        break;
    case Qt::TopEdge:
    case Qt::BottomEdge:
        layoutHorizontalEdge(loadRequest.edge());
        break;
    default:
        layoutTopLeftItem();
        break;
    }
}

void QQuickTableViewPrivate::cancelLoadRequest()
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
    qCDebug(lcTableViewDelegateLifecycle()) << "rollback:" << rollbackItems << tableLayoutToString();
    unloadItems(rollbackItems);
}

void QQuickTableViewPrivate::processLoadRequest()
{
    Q_TABLEVIEW_ASSERT(loadRequest.isActive(), "");

    while (loadRequest.hasCurrentCell()) {
        QPoint cell = loadRequest.currentCell();
        FxTableItem *fxTableItem = loadFxTableItem(cell, loadRequest.incubationMode());

        if (!fxTableItem) {
            // Requested item is not yet ready. Just leave, and wait for this
            // function to be called again when the item is ready.
            return;
        }

        loadedItems.insert(modelIndexAtCell(cell), fxTableItem);
        loadRequest.moveToNextCell();
    }

    qCDebug(lcTableViewDelegateLifecycle()) << "all items loaded!";

    syncLoadedTableFromLoadRequest();
    layoutTableEdgeFromLoadRequest();

    syncLoadedTableRectFromLoadedTable();
    enforceTableAtOrigin();
    updateContentWidth();
    updateContentHeight();

    loadRequest.markAsDone();
    drainReusePoolAfterLoadRequest();

    qCDebug(lcTableViewDelegateLifecycle()) << "request completed! Table:" << tableLayoutToString();
}

void QQuickTableViewPrivate::beginRebuildTable()
{
    Q_Q(QQuickTableView);
    qCDebug(lcTableViewDelegateLifecycle());

    tableInvalid = false;
    tableRebuilding = true;

    releaseLoadedItems();
    loadedTable = QRect();
    loadedTableOuterRect = QRect();
    loadedTableInnerRect = QRect();
    contentSizeBenchMarkPoint = QPoint(-1, -1);

    q->setContentX(0);
    q->setContentY(0);

    calculateTableSize();
    loadInitialTopLeftItem();
    loadAndUnloadVisibleEdges();
}

void QQuickTableViewPrivate::endRebuildTable()
{
    tableRebuilding = false;

    if (rowHeightProvider.isNull() && columnWidthProvider.isNull()) {
        // Since we have no size providers, we need to calculate the size
        // of each row and column based on the size of the delegate items.
        // This couldn't be done while we were loading the initial rows and
        // columns, since during the process, we didn't have all the items
        // available yet for the calculation. So we mark that it needs to be
        // done now, from within updatePolish().
        columnRowPositionsInvalid = true;
    }

    qCDebug(lcTableViewDelegateLifecycle()) << tableLayoutToString();
}

void QQuickTableViewPrivate::loadInitialTopLeftItem()
{
    Q_TABLEVIEW_ASSERT(loadedItems.isEmpty(), "");

    if (tableSize.isEmpty())
        return;

    if (model->count() == 0)
        return;

    if (tableModel && !tableModel->delegate())
        return;

    // Load top-left item. After loaded, loadItemsInsideRect() will take
    // care of filling out the rest of the table.
    loadRequest.begin(QPoint(0, 0), QQmlIncubator::AsynchronousIfNested);
    processLoadRequest();
}

void QQuickTableViewPrivate::unloadEdge(Qt::Edge edge)
{
    unloadItems(rectangleEdge(loadedTable, edge));
    loadedTable = expandedRect(loadedTable, edge, -1);
    syncLoadedTableRectFromLoadedTable();
    qCDebug(lcTableViewDelegateLifecycle) << tableLayoutToString();
}

void QQuickTableViewPrivate::loadEdge(Qt::Edge edge, QQmlIncubator::IncubationMode incubationMode)
{
    QLine cellsToLoad = rectangleEdge(expandedRect(loadedTable, edge, 1), edge);
    loadRequest.begin(cellsToLoad, edge, incubationMode);
    processLoadRequest();
}

void QQuickTableViewPrivate::loadAndUnloadVisibleEdges()
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
        // this point means that the model is empty (or no delegate).
        return;
    }

    const QRectF unloadRect = hasBufferedItems ? bufferRect() : viewportRect;
    bool tableModified;

    do {
        tableModified = false;

        if (Qt::Edge edge = nextEdgeToUnload(unloadRect)) {
            tableModified = true;
            unloadEdge(edge);
        }

        if (Qt::Edge edge = nextEdgeToLoad(viewportRect)) {
            tableModified = true;
            loadEdge(edge, QQmlIncubator::AsynchronousIfNested);
            if (loadRequest.isActive())
                return;
        }
    } while (tableModified);

}

void QQuickTableViewPrivate::drainReusePoolAfterLoadRequest()
{
    Q_Q(QQuickTableView);

    if (reusableFlag == QQmlTableInstanceModel::NotReusable || !tableModel)
        return;

    if (q->verticalOvershoot() || q->horizontalOvershoot()) {
        // Don't drain while we're overshooting, since this will fill up the
        // pool, but we expect to reuse them all once the content item moves back.
        return;
    }

    // When loading edges, we don't want to drain the reuse pool too aggressively. Normally,
    // all the items in the pool are reused rapidly as the content view is flicked around
    // anyway. Even if the table is temporarily flicked to a section that contains fewer
    // cells than what used to be (e.g if the flicked-in rows are taller than average), it
    // still makes sense to keep all the items in circulation; Chances are, that soon enough,
    // thinner rows are flicked back in again (meaning that we can fit more items into the
    // view). But at the same time, if a delegate chooser is in use, the pool might contain
    // items created from different delegates. And some of those delegates might be used only
    // occasionally. So to avoid situations where an item ends up in the pool for too long, we
    // call drain after each load request, but with a sufficiently large pool time. (If an item
    // in the pool has a large pool time, it means that it hasn't been reused for an equal
    // amount of load cycles, and should be released).
    //
    // We calculate an appropriate pool time by figuring out what the minimum time must be to
    // not disturb frequently reused items. Since the number of items in a row might be higher
    // than in a column (or vice versa), the minimum pool time should take into account that
    // you might be flicking out a single row (filling up the pool), before you continue
    // flicking in several new columns (taking them out again, but now in smaller chunks). This
    // will increase the number of load cycles items are kept in the pool (poolTime), but still,
    // we shouldn't release them, as they are still being reused frequently.
    // To get a flexible maxValue (that e.g tolerates rows and columns being flicked
    // in with varying sizes, causing some items not to be resued immediately), we multiply the
    // value by 2. Note that we also add an extra +1 to the column count, because the number of
    // visible columns will fluctuate between +1/-1 while flicking.
    const int w = loadedTable.width();
    const int h = loadedTable.height();
    const int minTime = std::ceil(w > h ? qreal(w + 1) / h : qreal(h + 1) / w);
    const int maxTime = minTime * 2;
    tableModel->drainReusableItemsPool(maxTime);
}

void QQuickTableViewPrivate::loadBuffer()
{
    // Rather than making sure to stop the timer from all locations that can
    // violate the "buffering allowed" state, we just check that we're in the
    // right state here before we start buffering.
    if (cacheBuffer <= 0 || loadRequest.isActive() || loadedItems.isEmpty())
        return;

    qCDebug(lcTableViewDelegateLifecycle());
    const QRectF loadRect = bufferRect();
    while (Qt::Edge edge = nextEdgeToLoad(loadRect)) {
        loadEdge(edge, QQmlIncubator::Asynchronous);
        if (loadRequest.isActive())
            break;
    }

    hasBufferedItems = true;
}

void QQuickTableViewPrivate::unloadBuffer()
{
    if (!hasBufferedItems)
        return;

    qCDebug(lcTableViewDelegateLifecycle());
    hasBufferedItems = false;
    cacheBufferDelayTimer.stop();
    if (loadRequest.isActive())
        cancelLoadRequest();
    while (Qt::Edge edge = nextEdgeToUnload(viewportRect))
        unloadEdge(edge);
}

QRectF QQuickTableViewPrivate::bufferRect()
{
    return viewportRect.adjusted(-cacheBuffer, -cacheBuffer, cacheBuffer, cacheBuffer);
}

void QQuickTableViewPrivate::invalidateTable() {
    tableInvalid = true;
    if (loadRequest.isActive())
        cancelLoadRequest();
    q_func()->polish();
}

void QQuickTableViewPrivate::invalidateColumnRowPositions() {
    columnRowPositionsInvalid = true;
    q_func()->polish();
}

void QQuickTableViewPrivate::updatePolish()
{
    // Whenever something changes, e.g viewport moves, spacing is set to a
    // new value, model changes etc, this function will end up being called. Here
    // we check what needs to be done, and load/unload cells accordingly.
    Q_Q(QQuickTableView);

    Q_TABLEVIEW_ASSERT(!polishing, "recursive updatePolish() calls are not allowed!");
    QBoolBlocker polishGuard(polishing, true);

    if (loadRequest.isActive()) {
        // We're currently loading items async to build a new edge in the table. We see the loading
        // as an atomic operation, which means that we don't continue doing anything else until all
        // items have been received and laid out. Note that updatePolish is then called once more
        // after the loadRequest has completed to handle anything that might have occurred in-between.
        return;
    }

    // viewportRect describes the part of the content view that is actually visible. Since a
    // negative width/height can happen (e.g during start-up), we check for this to avoid rebuilding
    // the table (and e.g calculate initial row/column sizes) based on a premature viewport rect.
    viewportRect = QRectF(q->contentX(), q->contentY(), q->width(), q->height());
    if (!viewportRect.isValid())
        return;

    if (tableInvalid) {
        beginRebuildTable();
        if (loadRequest.isActive())
            return;
    }

    if (tableRebuilding)
        endRebuildTable();

    if (loadedItems.isEmpty()) {
        qCDebug(lcTableViewDelegateLifecycle()) << "no items loaded, meaning empty model or no delegate";
        return;
    }

    if (columnRowPositionsInvalid)
        relayoutTable();

    if (hasBufferedItems && nextEdgeToLoad(viewportRect)) {
        // We are about to load more edges, so trim down the table as much
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

void QQuickTableViewPrivate::createWrapperModel()
{
    Q_Q(QQuickTableView);
    // When the assigned model is not an instance model, we create a wrapper
    // model (QQmlTableInstanceModel) that keeps a pointer to both the
    // assigned model and the assigned delegate. This model will give us a
    // common interface to any kind of model (js arrays, QAIM, number etc), and
    // help us create delegate instances.
    tableModel = new QQmlTableInstanceModel(qmlContext(q));
    model = tableModel;
}

void QQuickTableViewPrivate::itemCreatedCallback(int modelIndex, QObject*)
{
    if (blockItemCreatedCallback)
        return;

    qCDebug(lcTableViewDelegateLifecycle) << "item done loading:"
        << cellAtModelIndex(modelIndex);

    // Since the item we waited for has finished incubating, we can
    // continue with the load request. processLoadRequest will
    // ask the model for the requested item once more, which will be
    // quick since the model has cached it.
    processLoadRequest();
    loadAndUnloadVisibleEdges();
    updatePolish();
}

void QQuickTableViewPrivate::initItemCallback(int modelIndex, QObject *object)
{
    Q_UNUSED(modelIndex);
    Q_Q(QQuickTableView);

    if (auto item = qmlobject_cast<QQuickItem*>(object))
        item->setParentItem(q->contentItem());

    if (auto attached = getAttachedObject(object))
        attached->setView(q);
}

void QQuickTableViewPrivate::itemPooledCallback(int modelIndex, QObject *object)
{
    Q_UNUSED(modelIndex);

    if (auto attached = getAttachedObject(object))
        emit attached->pooled();
}

void QQuickTableViewPrivate::itemReusedCallback(int modelIndex, QObject *object)
{
    Q_UNUSED(modelIndex);

    if (auto attached = getAttachedObject(object))
        emit attached->reused();
}

void QQuickTableViewPrivate::connectToModel()
{
    Q_TABLEVIEW_ASSERT(model, "");

    QObjectPrivate::connect(model, &QQmlInstanceModel::createdItem, this, &QQuickTableViewPrivate::itemCreatedCallback);
    QObjectPrivate::connect(model, &QQmlInstanceModel::initItem, this, &QQuickTableViewPrivate::initItemCallback);

    if (tableModel) {
        QObjectPrivate::connect(tableModel, &QQmlTableInstanceModel::itemPooled, this, &QQuickTableViewPrivate::itemPooledCallback);
        QObjectPrivate::connect(tableModel, &QQmlTableInstanceModel::itemReused, this, &QQuickTableViewPrivate::itemReusedCallback);
    }

    if (auto const aim = model->abstractItemModel()) {
        // When the model exposes a QAIM, we connect to it directly. This means that if the current model is
        // a QQmlDelegateModel, we just ignore all the change sets it emits. In most cases, the model will instead
        // be our own QQmlTableInstanceModel, which doesn't bother creating change sets at all. For models that are
        // not based on QAIM (like QQmlObjectModel, QQmlListModel, javascript arrays etc), there is currently no way
        // to modify the model at runtime without also re-setting the model on the view.
        connect(aim, &QAbstractItemModel::rowsMoved, this, &QQuickTableViewPrivate::rowsMovedCallback);
        connect(aim, &QAbstractItemModel::columnsMoved, this, &QQuickTableViewPrivate::columnsMovedCallback);
        connect(aim, &QAbstractItemModel::rowsInserted, this, &QQuickTableViewPrivate::rowsInsertedCallback);
        connect(aim, &QAbstractItemModel::rowsRemoved, this, &QQuickTableViewPrivate::rowsRemovedCallback);
        connect(aim, &QAbstractItemModel::columnsInserted, this, &QQuickTableViewPrivate::columnsInsertedCallback);
        connect(aim, &QAbstractItemModel::columnsRemoved, this, &QQuickTableViewPrivate::columnsRemovedCallback);
        connect(aim, &QAbstractItemModel::modelReset, this, &QQuickTableViewPrivate::modelResetCallback);
    } else {
        QObjectPrivate::connect(model, &QQmlInstanceModel::modelUpdated, this, &QQuickTableViewPrivate::modelUpdated);
    }
}

void QQuickTableViewPrivate::disconnectFromModel()
{
    Q_TABLEVIEW_ASSERT(model, "");

    QObjectPrivate::disconnect(model, &QQmlInstanceModel::createdItem, this, &QQuickTableViewPrivate::itemCreatedCallback);
    QObjectPrivate::disconnect(model, &QQmlInstanceModel::initItem, this, &QQuickTableViewPrivate::initItemCallback);

    if (tableModel) {
        QObjectPrivate::disconnect(tableModel, &QQmlTableInstanceModel::itemPooled, this, &QQuickTableViewPrivate::itemPooledCallback);
        QObjectPrivate::disconnect(tableModel, &QQmlTableInstanceModel::itemReused, this, &QQuickTableViewPrivate::itemReusedCallback);
    }

    if (auto const aim = model->abstractItemModel()) {
        disconnect(aim, &QAbstractItemModel::rowsMoved, this, &QQuickTableViewPrivate::rowsMovedCallback);
        disconnect(aim, &QAbstractItemModel::columnsMoved, this, &QQuickTableViewPrivate::columnsMovedCallback);
        disconnect(aim, &QAbstractItemModel::rowsInserted, this, &QQuickTableViewPrivate::rowsInsertedCallback);
        disconnect(aim, &QAbstractItemModel::rowsRemoved, this, &QQuickTableViewPrivate::rowsRemovedCallback);
        disconnect(aim, &QAbstractItemModel::columnsInserted, this, &QQuickTableViewPrivate::columnsInsertedCallback);
        disconnect(aim, &QAbstractItemModel::columnsRemoved, this, &QQuickTableViewPrivate::columnsRemovedCallback);
        disconnect(aim, &QAbstractItemModel::modelReset, this, &QQuickTableViewPrivate::modelResetCallback);
    } else {
        QObjectPrivate::disconnect(model, &QQmlInstanceModel::modelUpdated, this, &QQuickTableViewPrivate::modelUpdated);
    }
}

void QQuickTableViewPrivate::modelUpdated(const QQmlChangeSet &changeSet, bool reset)
{
    Q_UNUSED(changeSet);
    Q_UNUSED(reset);

    Q_TABLEVIEW_ASSERT(!model->abstractItemModel(), "");
    invalidateTable();
}

void QQuickTableViewPrivate::rowsMovedCallback(const QModelIndex &parent, int, int, const QModelIndex &, int )
{
    if (parent != QModelIndex())
        return;

    invalidateTable();
}

void QQuickTableViewPrivate::columnsMovedCallback(const QModelIndex &parent, int, int, const QModelIndex &, int)
{
    if (parent != QModelIndex())
        return;

    invalidateTable();
}

void QQuickTableViewPrivate::rowsInsertedCallback(const QModelIndex &parent, int, int)
{
    if (parent != QModelIndex())
        return;

    invalidateTable();
}

void QQuickTableViewPrivate::rowsRemovedCallback(const QModelIndex &parent, int, int)
{
    if (parent != QModelIndex())
        return;

    invalidateTable();
}

void QQuickTableViewPrivate::columnsInsertedCallback(const QModelIndex &parent, int, int)
{
    if (parent != QModelIndex())
        return;

    invalidateTable();
}

void QQuickTableViewPrivate::columnsRemovedCallback(const QModelIndex &parent, int, int)
{
    if (parent != QModelIndex())
        return;

    invalidateTable();
}

void QQuickTableViewPrivate::modelResetCallback()
{
    invalidateTable();
}

bool QQuickTableViewPrivate::updatePolishIfPossible() const
{
    if (polishing || !q_func()->isComponentComplete()) {
        // We shouldn't recurse into updatePolish(). And we shouldn't
        // build the table before the component is complete.
        return false;
    }

    const_cast<QQuickTableViewPrivate *>(this)->updatePolish();
    return true;
}

QQuickTableView::QQuickTableView(QQuickItem *parent)
    : QQuickFlickable(*(new QQuickTableViewPrivate), parent)
{
    connect(this, &QQuickFlickable::contentWidthChanged, this, &QQuickTableView::contentWidthOverrideChanged);
    connect(this, &QQuickFlickable::contentHeightChanged, this, &QQuickTableView::contentHeightOverrideChanged);
}

int QQuickTableView::rows() const
{
    return d_func()->tableSize.height();
}

int QQuickTableView::columns() const
{
    return d_func()->tableSize.width();
}

qreal QQuickTableView::rowSpacing() const
{
    return d_func()->cellSpacing.height();
}

void QQuickTableView::setRowSpacing(qreal spacing)
{
    Q_D(QQuickTableView);
    if (qt_is_nan(spacing) || !qt_is_finite(spacing) || spacing < 0)
        return;
    if (qFuzzyCompare(d->cellSpacing.height(), spacing))
        return;

    d->cellSpacing.setHeight(spacing);
    d->invalidateColumnRowPositions();
    emit rowSpacingChanged();
}

qreal QQuickTableView::columnSpacing() const
{
    return d_func()->cellSpacing.width();
}

void QQuickTableView::setColumnSpacing(qreal spacing)
{
    Q_D(QQuickTableView);
    if (qt_is_nan(spacing) || !qt_is_finite(spacing) || spacing < 0)
        return;
    if (qFuzzyCompare(d->cellSpacing.width(), spacing))
        return;

    d->cellSpacing.setWidth(spacing);
    d->invalidateColumnRowPositions();
    emit columnSpacingChanged();
}

qreal QQuickTableView::topMargin() const
{
    return d_func()->tableMargins.top();
}

void QQuickTableView::setTopMargin(qreal margin)
{
    Q_D(QQuickTableView);
    if (qt_is_nan(margin))
        return;
    if (qFuzzyCompare(d->tableMargins.top(), margin))
        return;

    d->tableMargins.setTop(margin);
    d->invalidateColumnRowPositions();
    emit topMarginChanged();
}

qreal QQuickTableView::bottomMargin() const
{
    return d_func()->tableMargins.bottom();
}

void QQuickTableView::setBottomMargin(qreal margin)
{
    Q_D(QQuickTableView);
    if (qt_is_nan(margin))
        return;
    if (qFuzzyCompare(d->tableMargins.bottom(), margin))
        return;

    d->tableMargins.setBottom(margin);
    d->invalidateColumnRowPositions();
    emit bottomMarginChanged();
}

qreal QQuickTableView::leftMargin() const
{
    return d_func()->tableMargins.left();
}

void QQuickTableView::setLeftMargin(qreal margin)
{
    Q_D(QQuickTableView);
    if (qt_is_nan(margin))
        return;
    if (qFuzzyCompare(d->tableMargins.left(), margin))
        return;

    d->tableMargins.setLeft(margin);
    d->invalidateColumnRowPositions();
    emit leftMarginChanged();
}

qreal QQuickTableView::rightMargin() const
{
    return d_func()->tableMargins.right();
}

void QQuickTableView::setRightMargin(qreal margin)
{
    Q_D(QQuickTableView);
    if (qt_is_nan(margin))
        return;
    if (qFuzzyCompare(d->tableMargins.right(), margin))
        return;

    d->tableMargins.setRight(margin);
    d->invalidateColumnRowPositions();
    emit rightMarginChanged();
}

QJSValue QQuickTableView::rowHeightProvider() const
{
    return d_func()->rowHeightProvider;
}

void QQuickTableView::setRowHeightProvider(QJSValue provider)
{
    Q_D(QQuickTableView);
    if (provider.strictlyEquals(d->rowHeightProvider))
        return;

    d->rowHeightProvider = provider;
    d->invalidateTable();
    emit rowHeightProviderChanged();
}

QJSValue QQuickTableView::columnWidthProvider() const
{
    return d_func()->columnWidthProvider;
}

void QQuickTableView::setColumnWidthProvider(QJSValue provider)
{
    Q_D(QQuickTableView);
    if (provider.strictlyEquals(d->columnWidthProvider))
        return;

    d->columnWidthProvider = provider;
    d->invalidateTable();
    emit columnWidthProviderChanged();
}

QVariant QQuickTableView::model() const
{
    return d_func()->modelVariant;
}

void QQuickTableView::setModel(const QVariant &newModel)
{
    Q_D(QQuickTableView);

    if (d->model)
        d->disconnectFromModel();

    d->modelVariant = newModel;
    QVariant effectiveModelVariant = d->modelVariant;
    if (effectiveModelVariant.userType() == qMetaTypeId<QJSValue>())
        effectiveModelVariant = effectiveModelVariant.value<QJSValue>().toVariant();

    const auto instanceModel = qobject_cast<QQmlInstanceModel *>(qvariant_cast<QObject*>(effectiveModelVariant));

    if (instanceModel) {
        if (d->tableModel) {
            delete d->tableModel;
            d->tableModel = nullptr;
        }
        d->model = instanceModel;
    } else {
        if (!d->tableModel)
            d->createWrapperModel();
        d->tableModel->setModel(effectiveModelVariant);
    }

    d->connectToModel();
    d->invalidateTable();
    emit modelChanged();
}

QQmlComponent *QQuickTableView::delegate() const
{
    Q_D(const QQuickTableView);
    if (d->tableModel)
        return d->tableModel->delegate();

    return nullptr;
}

void QQuickTableView::setDelegate(QQmlComponent *newDelegate)
{
    Q_D(QQuickTableView);
    if (newDelegate == delegate())
        return;

    if (!d->tableModel)
        d->createWrapperModel();

    d->tableModel->setDelegate(newDelegate);
    d->invalidateTable();

    emit delegateChanged();
}

bool QQuickTableView::reuseItems() const
{
    return bool(d_func()->reusableFlag == QQmlTableInstanceModel::Reusable);
}

void QQuickTableView::setReuseItems(bool reuse)
{
    Q_D(QQuickTableView);
    if (reuseItems() == reuse)
        return;

    d->reusableFlag = reuse ? QQmlTableInstanceModel::Reusable : QQmlTableInstanceModel::NotReusable;

    emit reuseItemsChanged();
}

qreal QQuickTableView::explicitContentWidth() const
{
    Q_D(const QQuickTableView);

    if (d->tableInvalid && d->explicitContentWidth.isNull) {
        // The table is pending to be rebuilt. Since we don't
        // know the contentWidth before this is done, we do the
        // rebuild now, instead of waiting for the polish event.
       d->updatePolishIfPossible();
    }

    return contentWidth();
}

void QQuickTableView::setExplicitContentWidth(qreal width)
{
    Q_D(QQuickTableView);
    d->explicitContentWidth = width;
    if (width == contentWidth())
        return;

    setContentWidth(width);
}

qreal QQuickTableView::explicitContentHeight() const
{
    Q_D(const QQuickTableView);

    if (d->tableInvalid && d->explicitContentHeight.isNull) {
        // The table is pending to be rebuilt. Since we don't
        // know the contentHeight before this is done, we do the
        // rebuild now, instead of waiting for the polish event.
       d->updatePolishIfPossible();
    }

    return contentHeight();
}

void QQuickTableView::setExplicitContentHeight(qreal height)
{
    Q_D(QQuickTableView);
    d->explicitContentHeight = height;
    if (height == contentHeight())
        return;

    setContentHeight(height);
}

void QQuickTableView::forceLayout()
{
    Q_D(QQuickTableView);
    d->columnRowPositionsInvalid = true;
    if (!d->updatePolishIfPossible())
        polish();
}

QQuickTableViewAttached *QQuickTableView::qmlAttachedProperties(QObject *obj)
{
    return new QQuickTableViewAttached(obj);
}

void QQuickTableView::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickTableView);
    QQuickFlickable::geometryChanged(newGeometry, oldGeometry);

    if (d->tableModel) {
        // When the view changes size, we force the pool to
        // shrink by releasing all pooled items.
        d->tableModel->drainReusableItemsPool(0);
    }

    polish();
}

void QQuickTableView::viewportMoved(Qt::Orientations orientation)
{
    QQuickFlickable::viewportMoved(orientation);

    // Calling polish() will schedule a polish event. But while the user is flicking, several
    // mouse events will be handled before we get an updatePolish() call. And the updatePolish()
    // call will only see the last mouse position. This results in a stuttering flick experience
    // (especially on windows). We improve on this by calling updatePolish() directly. But this
    // has the pitfall that we open up for recursive callbacks. E.g while inside updatePolish(), we
    // load/unload items, and emit signals. The application can listen to those signals and set a
    // new contentX/Y on the flickable. So we need to guard for this, to avoid unexpected behavior.
    if (!d_func()->updatePolishIfPossible())
        polish();
}

void QQuickTableView::componentComplete()
{
    Q_D(QQuickTableView);

    if (!d->model)
        setModel(QVariant());

    QQuickFlickable::componentComplete();
}

#include "moc_qquicktableview_p.cpp"

QT_END_NAMESPACE
