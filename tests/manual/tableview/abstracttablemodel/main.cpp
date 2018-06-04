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

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QAbstractTableModel>
#include <QtDebug>

class TestTableModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(int rowCount READ rowCount WRITE setRowCount NOTIFY rowCountChanged)
    Q_PROPERTY(int columnCount READ columnCount WRITE setColumnCount NOTIFY columnCountChanged)

public:
    TestTableModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {
        connect(this, &QAbstractTableModel::modelReset, this, &TestTableModel::rebuildModel);
    }

    int rowCount(const QModelIndex & = QModelIndex()) const override { return m_rows; }
    void setRowCount(int count) { beginResetModel(); m_rows = count; emit rowCountChanged(); endResetModel(); }

    int columnCount(const QModelIndex & = QModelIndex()) const override { return m_cols; }
    void setColumnCount(int count) { beginResetModel(); m_cols = count; emit columnCountChanged(); endResetModel(); }

    void rebuildModel()
    {
        m_modelData = QVector<QVector<QString>>(m_cols);
        for (int x = 0; x < m_cols; ++x) {
            m_modelData[x] = QVector<QString>(m_rows);
            for (int y = 0; y < m_rows; ++y)
                m_modelData[x][y] = QStringLiteral("[D %1, %2]").arg(x).arg(y);
        }
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const
    {
        Q_UNUSED(orientation);
        Q_UNUSED(role);
        return QStringLiteral("Column header");
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || role != Qt::DisplayRole)
            return QVariant();
        return m_modelData[index.column()][index.row()];
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return { {Qt::DisplayRole, "display"} };
    }

    Q_INVOKABLE void addNewRow(int row)
    {
        insertRow(row);
    }

    Q_INVOKABLE void addNewColumn(int column)
    {
        insertColumn(column);
    }

    Q_INVOKABLE void setCellData(int row, int column, const QString &data)
    {
        m_modelData[column][row] = data;
        QModelIndex cellIndex = createIndex(row, column);
        emit dataChanged(cellIndex, cellIndex);
    }

    bool insertRows(int row, int count, const QModelIndex &parent) override
    {
        if (count < 1 || row < 0 || row > rowCount(parent))
            return false;

        beginInsertRows(QModelIndex(), row, row + count - 1);

        Q_ASSERT(count == 1);
        m_rows++;
        for (int x = 0; x < m_cols; ++x)
            m_modelData[x].insert(row, QStringLiteral("[R %1, %2]").arg(x).arg(row));

        endInsertRows();

        return true;
    }

    bool insertColumns(int column, int count, const QModelIndex &parent) override
    {
        if (column < 1 || column < 0 || column > columnCount(parent))
            return false;

        beginInsertColumns(QModelIndex(), column, column + count - 1);

        Q_ASSERT(count == 1);
        m_cols++;
        auto newColumn = QVector<QString>(m_rows);
        for (int y = 0; y < m_rows; ++y)
            newColumn[y] = QStringLiteral("[C %1, %2]").arg(column).arg(y);
        m_modelData.insert(column, newColumn);

        endInsertColumns();

        return true;
    }

signals:
    void rowCountChanged();
    void columnCountChanged();

private:
    int m_rows = 0;
    int m_cols = 0;

    QVector<QVector<QString>> m_modelData;
};

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    qmlRegisterType<TestTableModel>("TestTableModel", 0, 1, "TestTableModel");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    return app.exec();
}

#include "main.moc"
