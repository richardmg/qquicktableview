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
};

class QQuickTableViewPrivate : public QQuickAbstractItemViewPrivate
{
    Q_DECLARE_PUBLIC(QQuickTableView)

public:
    QQuickTableViewPrivate();

    int rowAt(int index) const;
    int columnAt(int index) const;
    int indexAt(int row, int column) const;
    FxViewItem *visibleItemAt(int row, int column) const;

    qreal rowPos(int row) const;
    qreal rowHeight(int row) const;
    qreal columnPos(int column) const;
    qreal columnWidth(int column) const;
    void updateAverageSize();

    void positionViewAtIndex(int index, int mode) override { Q_UNUSED(index); Q_UNUSED(mode); }
    bool applyModelChanges() override { return false; }
    Qt::Orientation layoutOrientation() const override { return Qt::Vertical; }
    bool isContentFlowReversed() const override { return false; }
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
    QSizeF averageSize;
};

QQuickTableViewPrivate::QQuickTableViewPrivate()
    : rows(-1),
      columns(-1),
      visibleRows(0),
      visibleColumns(0)
{
}

int QQuickTableViewPrivate::rowAt(int index) const
{
    if (index < 0 || index >= itemCount || rows <= 0)
        return -1;
    return index % rows;
}

int QQuickTableViewPrivate::columnAt(int index) const
{
    if (index < 0 || index >= itemCount || rows <= 0)
        return -1;
    return index / rows;
}

int QQuickTableViewPrivate::indexAt(int row, int column) const
{
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

void QQuickTableViewPrivate::visibleItemsChanged()
{
    updateAverageSize();
}

FxViewItem *QQuickTableViewPrivate::newViewItem(int index, QQuickItem *item)
{
    Q_Q(QQuickTableView);
    Q_UNUSED(index);
    return new FxTableItemSG(item, q, false);
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
