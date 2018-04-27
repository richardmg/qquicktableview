/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>

#include <QtQuick/qquickview.h>
#include <QtQuick/private/qquicktableview_p.h>

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlexpression.h>
#include <QtQml/qqmlincubator.h>
#include <QtQml/private/qqmlobjectmodel_p.h>
#include <QtQml/private/qqmllistmodel_p.h>
#include <QtQml/private/qqmldelegatemodel_p.h>

#include "testmodel.h"

#include "../../shared/util.h"
#include "../shared/viewtestutil.h"
#include "../shared/visualtestutil.h"

using namespace QQuickViewTestUtil;
using namespace QQuickVisualTestUtil;

#define CREATE_VAR_FROM_PROPERTY(name, T) \
    T *name = view->rootObject()->property(#name).value<T *>(); \
    QVERIFY(name)

#define LOAD_TABLEVIEW(fileName) \
    QScopedPointer<QQuickView> view(createView()); \
    view->setSource(testFileUrl(fileName)); \
    view->show(); \
    QVERIFY(QTest::qWaitForWindowActive(view.data())); \
    CREATE_VAR_FROM_PROPERTY(tableView, QQuickTableView)

class tst_QQuickTableView : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_QQuickTableView();

    QList<QQuickItem *> findDelegateItems(QQuickTableView *tableView);
    QQuickTableViewAttached *getAttachedObject(const QObject *object) const;

private slots:
    void initTestCase() override;

    void setAndGetModel_data();
    void setAndGetModel();
    void countDelegateItems_data();
    void countDelegateItems();
    void checkLayoutOfVisibleDelegateItems_data();
    void checkLayoutOfVisibleDelegateItems();
};

tst_QQuickTableView::tst_QQuickTableView()
{
}

void tst_QQuickTableView::initTestCase()
{
    QQmlDataTest::initTestCase();
    qmlRegisterType<TestModel>("TestModel", 0, 1, "TestModel");
}

QList<QQuickItem *> tst_QQuickTableView::findDelegateItems(QQuickTableView *tableView)
{
    static const QString delegateObjectName = QStringLiteral("tableViewDelegate");
    // Call processEvents to process any pending polish event posted for tableView
    qApp->processEvents();
    return findItems<QQuickItem>(tableView, delegateObjectName);
}

QQuickTableViewAttached *tst_QQuickTableView::getAttachedObject(const QObject *object) const
{
    QObject *attachedObject = qmlAttachedPropertiesObject<QQuickTableView>(object);
    return static_cast<QQuickTableViewAttached *>(attachedObject);
}

void tst_QQuickTableView::setAndGetModel_data()
{
    QTest::addColumn<QVariant>("model");

    QTest::newRow("QAIM 1x1") << TestModelAsVariant(1, 1);
    QTest::newRow("Number model 1") << QVariant::fromValue(1);
    QTest::newRow("QStringList 1") << QVariant::fromValue(QStringList() << "one");
}

void tst_QQuickTableView::setAndGetModel()
{
    // Test that we can set and get different kind of models
    QFETCH(QVariant, model);
    LOAD_TABLEVIEW("plaintableview.qml");

    tableView->setModel(model);
    QCOMPARE(model, tableView->model());
}

void tst_QQuickTableView::countDelegateItems_data()
{
    QTest::addColumn<QVariant>("model");
    QTest::addColumn<int>("count");

    QTest::newRow("QAIM 0x0") << TestModelAsVariant(0, 0) << 0;
    QTest::newRow("QAIM 1x1") << TestModelAsVariant(1, 1) << 1;
    QTest::newRow("QAIM 2x1") << TestModelAsVariant(2, 1) << 2;
    QTest::newRow("QAIM 4x1") << TestModelAsVariant(4, 1) << 4;
    QTest::newRow("QAIM 1x2") << TestModelAsVariant(1, 2) << 2;
    QTest::newRow("QAIM 1x4") << TestModelAsVariant(1, 4) << 4;
    QTest::newRow("QAIM 2x2") << TestModelAsVariant(2, 2) << 4;
    QTest::newRow("QAIM 4x4") << TestModelAsVariant(4, 4) << 16;

    QTest::newRow("Number model 0") << QVariant::fromValue(0) << 0;
    QTest::newRow("Number model 1") << QVariant::fromValue(1) << 1;
    QTest::newRow("Number model 4") << QVariant::fromValue(4) << 4;

    QTest::newRow("QStringList 0") << QVariant::fromValue(QStringList()) << 0;
    QTest::newRow("QStringList 1") << QVariant::fromValue(QStringList() << "one") << 1;
    QTest::newRow("QStringList 4") << QVariant::fromValue(QStringList() << "one" << "two" << "three" << "four") << 4;
}

void tst_QQuickTableView::countDelegateItems()
{
    // Assign different models of various sizes, and check that the number of
    // delegate items in the view matches the size of the model. Note that for
    // this test to be valid, all items must be within the visible area of the view.
    QFETCH(QVariant, model);
    QFETCH(int, count);
    LOAD_TABLEVIEW("plaintableview.qml");

    tableView->setModel(model);
    auto const items = findDelegateItems(tableView);
    QCOMPARE(items.count(), count);
}

void tst_QQuickTableView::checkLayoutOfVisibleDelegateItems_data()
{
    QTest::addColumn<QVariant>("model");
    QTest::addColumn<int>("rows");
    QTest::addColumn<int>("columns");
    QTest::addColumn<qreal>("rowSpacing");
    QTest::addColumn<qreal>("columnSpacing");

    QTest::newRow("QAIM 1x1 1,1") << TestModelAsVariant(1, 1) << 1 << 1 << 1. << 1.;
    QTest::newRow("QAIM 5x5 0,0") << TestModelAsVariant(5, 5) << 5 << 5 << 0. << 0.;
    QTest::newRow("QAIM 5x5 1,0") << TestModelAsVariant(5, 5) << 5 << 5 << 1. << 0.;
    QTest::newRow("QAIM 5x5 0,1") << TestModelAsVariant(5, 5) << 5 << 5 << 0. << 1.;
}

void tst_QQuickTableView::checkLayoutOfVisibleDelegateItems()
{
    // Check that the geometry of the delegate items are correct
    QFETCH(QVariant, model);
    QFETCH(int, rows);
    QFETCH(int, columns);
    QFETCH(qreal, rowSpacing);
    QFETCH(qreal, columnSpacing);
    LOAD_TABLEVIEW("plaintableview.qml");

    tableView->setModel(model);
    tableView->setRowSpacing(rowSpacing);
    tableView->setColumnSpacing(columnSpacing);

    auto const items = findDelegateItems(tableView);
    QVERIFY(!items.isEmpty());

    const QQuickItem *firstItem = items.at(0);
    qreal width = firstItem->width();
    qreal height = firstItem->height();
    QVERIFY(width > 0);
    QVERIFY(height > 0);

    for (int i = 0; i < rows * columns; ++i) {
        const QQuickItem *item = items.at(i);
        auto attached = getAttachedObject(item);
        int row = attached->row();
        int column = attached->column();
        QVERIFY(row >= 0);
        QVERIFY(column >= 0);
        qreal x = column * (width + columnSpacing);
        qreal y = row * (height + rowSpacing);
        QCOMPARE(item->x(), x);
        QCOMPARE(item->y(), y);
        QCOMPARE(item->width(), width);
        QCOMPARE(item->height(), height);
    }
}

QTEST_MAIN(tst_QQuickTableView)

#include "tst_qquicktableview.moc"
