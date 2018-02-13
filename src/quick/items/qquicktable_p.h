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

#ifndef QQUICKTABLE_P_H
#define QQUICKTABLE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qpointer.h>
#include <QtQuick/private/qtquickglobal_p.h>
#include "qquickitem.h"

QT_REQUIRE_CONFIG(quick_table);

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcTableDelegateLifecycle)

class QQuickTableAttached;
class QQuickTablePrivate;
class QQmlChangeSet;

class Q_AUTOTEST_EXPORT QQuickTable : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(int rows READ rows WRITE setRows RESET resetRows NOTIFY rowsChanged)
    Q_PROPERTY(int columns READ columns WRITE setColumns RESET resetColumns NOTIFY columnsChanged)
    Q_PROPERTY(qreal rowSpacing READ rowSpacing WRITE setRowSpacing NOTIFY rowSpacingChanged)
    Q_PROPERTY(qreal columnSpacing READ columnSpacing WRITE setColumnSpacing NOTIFY columnSpacingChanged)
    Q_PROPERTY(int cacheBuffer READ cacheBuffer WRITE setCacheBuffer NOTIFY cacheBufferChanged)

    Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QQmlComponent *delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)

    Q_CLASSINFO("DefaultProperty", "data")

public:
    QQuickTable(QQuickItem *parent = nullptr);

    int rows() const;
    void setRows(int rows);
    void resetRows();

    int columns() const;
    void setColumns(int columns);
    void resetColumns();

    qreal rowSpacing() const;
    void setRowSpacing(qreal spacing);

    qreal columnSpacing() const;
    void setColumnSpacing(qreal spacing);

    int cacheBuffer() const;
    void setCacheBuffer(int newBuffer);

    QVariant model() const;
    void setModel(const QVariant &newModel);

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *);

    static QQuickTableAttached *qmlAttachedProperties(QObject *);
    void componentComplete() override;

Q_SIGNALS:
    void rowsChanged();
    void columnsChanged();
    void rowSpacingChanged();
    void columnSpacingChanged();
    void cacheBufferChanged();
    void modelChanged();
    void delegateChanged();

protected Q_SLOTS:
    void initItemCallback(int modelIndex, QObject *item);
    void itemCreatedCallback(int modelIndex, QObject *object);
    void modelUpdated(const QQmlChangeSet &changeSet, bool reset);

private:
    Q_DISABLE_COPY(QQuickTable)
    Q_DECLARE_PRIVATE(QQuickTable)
};

class Q_QUICK_PRIVATE_EXPORT QQuickTableAttached : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QQuickTable *table READ table NOTIFY tableChanged)
    Q_PROPERTY(int row READ row NOTIFY rowChanged)
    Q_PROPERTY(int column READ column NOTIFY columnChanged)

public:
    QQuickTableAttached(QObject *parent)
        : QObject(parent) {}

    QQuickTable *table() const { return m_table; }
    void setTable(QQuickTable *newTable) {
        if (newTable == m_table)
            return;
        m_table = newTable;
        Q_EMIT tableChanged();
    }

    int row() const { return m_row; }
    void setRow(int newRow) {
        if (newRow == m_row)
            return;
        m_row = newRow;
        Q_EMIT rowChanged();
    }

    int column() const { return m_column; }
    void setColumn(int newColumn) {
        if (newColumn == m_column)
            return;
        m_column = newColumn;
        Q_EMIT columnChanged();
    }

Q_SIGNALS:
    void tableChanged();
    void rowChanged();
    void columnChanged();

private:
    QPointer<QQuickTable> m_table;
    int m_row = -1;
    int m_column = -1;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickTable)
QML_DECLARE_TYPEINFO(QQuickTable, QML_HAS_ATTACHED_PROPERTIES)

#endif // QQUICKTABLE_P_H
