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

static const QString kDelegateObjectName = QStringLiteral("tableViewDelegate");

Q_DECLARE_METATYPE(QMarginsF);

#define LOAD_TABLEVIEW(fileName) \
    QScopedPointer<QQuickView> view(createView()); \
    view->setSource(testFileUrl(fileName)); \
    view->show(); \
    QVERIFY(QTest::qWaitForWindowActive(view.data())); \
    auto tableView = view->rootObject()->property("tableView").value<QQuickTableView *>()

#define GET_DELEGATE_ITEMS(varName) \
    view->rootObject()->property("delegatesCreated").setValue(false); \
    QTRY_VERIFY(view->rootObject()->property("delegatesCreated").value<bool>()); \
    auto varName = findItems<QQuickItem>(tableView, kDelegateObjectName);

class tst_QQuickTableView : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_QQuickTableView();

    QQuickTableViewAttached *getAttachedObject(const QObject *object) const;

private slots:
    void initTestCase() override;

    void setAndGetModel_data();
    void setAndGetModel();
    void emptyModel_data();
    void emptyModel();
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

void tst_QQuickTableView::emptyModel_data()
{
    QTest::addColumn<QVariant>("model");

    QTest::newRow("QAIM") << TestModelAsVariant(0, 0);
    QTest::newRow("Number model") << QVariant::fromValue(0);
    QTest::newRow("QStringList") << QVariant::fromValue(QStringList());
}

void tst_QQuickTableView::emptyModel()
{
    // Check that if we assign an empty model to
    // TableView, no delegate items will be created.
    QFETCH(QVariant, model);
    LOAD_TABLEVIEW("plaintableview.qml");

    tableView->setModel(model);
    QTRY_COMPARE_WITH_TIMEOUT(view->rootObject()->property("delegatesCreated").value<bool>(), 0, 500);
}

void tst_QQuickTableView::countDelegateItems_data()
{
    QTest::addColumn<QVariant>("model");
    QTest::addColumn<int>("count");

    QTest::newRow("QAIM 1x1") << TestModelAsVariant(1, 1) << 1;
    QTest::newRow("QAIM 2x1") << TestModelAsVariant(2, 1) << 2;
    QTest::newRow("QAIM 4x1") << TestModelAsVariant(4, 1) << 4;
    QTest::newRow("QAIM 1x2") << TestModelAsVariant(1, 2) << 2;
    QTest::newRow("QAIM 1x4") << TestModelAsVariant(1, 4) << 4;
    QTest::newRow("QAIM 2x2") << TestModelAsVariant(2, 2) << 4;
    QTest::newRow("QAIM 4x4") << TestModelAsVariant(4, 4) << 16;

    QTest::newRow("Number model 1") << QVariant::fromValue(1) << 1;
    QTest::newRow("Number model 4") << QVariant::fromValue(4) << 4;

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
    GET_DELEGATE_ITEMS(items);
    QCOMPARE(items.count(), count);
}

void tst_QQuickTableView::checkLayoutOfVisibleDelegateItems_data()
{
    QTest::addColumn<QVariant>("model");
    QTest::addColumn<int>("rows");
    QTest::addColumn<int>("columns");
    QTest::addColumn<QSizeF>("spacing");
    QTest::addColumn<QMarginsF>("margins");

    // Check spacing together with different table setups
    QTest::newRow("QAIM 1x1 1,1") << TestModelAsVariant(1, 1) << 1 << 1 << QSizeF(1, 1) << QMarginsF(0, 0, 0, 0);
    QTest::newRow("QAIM 5x5 0,0") << TestModelAsVariant(5, 5) << 5 << 5 << QSizeF(0, 0) << QMarginsF(0, 0, 0, 0);
    QTest::newRow("QAIM 5x5 1,0") << TestModelAsVariant(5, 5) << 5 << 5 << QSizeF(1, 0) << QMarginsF(0, 0, 0, 0);
    QTest::newRow("QAIM 5x5 0,1") << TestModelAsVariant(5, 5) << 5 << 5 << QSizeF(0, 1) << QMarginsF(0, 0, 0, 0);

    // Check margins together with different table setups
    QTest::newRow("QAIM 1x1 1,1 5555") << TestModelAsVariant(1, 1) << 1 << 1 << QSizeF(1, 1) << QMarginsF(5, 5, 5, 5);
    QTest::newRow("QAIM 4x4 0,0 3333") << TestModelAsVariant(4, 4) << 4 << 4 << QSizeF(0, 0) << QMarginsF(3, 3, 3, 3);
    QTest::newRow("QAIM 4x4 2,2 1234") << TestModelAsVariant(4, 4) << 4 << 4 << QSizeF(2, 2) << QMarginsF(1, 2, 3, 4);
}

void tst_QQuickTableView::checkLayoutOfVisibleDelegateItems()
{
    // Check that the geometry of the delegate items are correct
    QFETCH(QVariant, model);
    QFETCH(int, rows);
    QFETCH(int, columns);
    QFETCH(QSizeF, spacing);
    QFETCH(QMarginsF, margins);
    LOAD_TABLEVIEW("plaintableview.qml");

    tableView->setModel(model);
    tableView->setRowSpacing(spacing.height());
    tableView->setColumnSpacing(spacing.width());
    tableView->setLeftMargin(margins.left());
    tableView->setTopMargin(margins.top());
    tableView->setRightMargin(margins.right());
    tableView->setBottomMargin(margins.bottom());

    GET_DELEGATE_ITEMS(items);

    const QQuickItem *firstItem = items.at(0);
    const QQuickItem *bottomRightItem = nullptr;
    qreal width = firstItem->width();
    qreal height = firstItem->height();
    QVERIFY(width > 0);
    QVERIFY(height > 0);

    // Check that all delegate items have the geometry we expect
    // them to have according to their attached row and column.
    for (int i = 0; i < rows * columns; ++i) {
        const QQuickItem *item = items.at(i);
        auto attached = getAttachedObject(item);
        int row = attached->row();
        int column = attached->column();
        QVERIFY(row >= 0);
        QVERIFY(column >= 0);
        qreal x = margins.left() + (column * (width + spacing.width()));
        qreal y = margins.top() + (row * (height + spacing.height()));
        QCOMPARE(item->x(), x);
        QCOMPARE(item->y(), y);
        QCOMPARE(item->width(), width);
        QCOMPARE(item->height(), height);

        if (row == rows -1 && column == columns - 1)
            bottomRightItem = item;
    }

    // Check that the space between the edge of the bottom right item to
    // the edge of the content view matches the margins. Top- and left
    // margins have already been tested in the for-loop above.
    QVERIFY(bottomRightItem);
    qreal rightSpace = tableView->contentWidth() - (bottomRightItem->x() + bottomRightItem->width());
    qreal bottomSpace = tableView->contentHeight() - (bottomRightItem->y() + bottomRightItem->height());
    QCOMPARE(rightSpace, margins.right());
    QCOMPARE(bottomSpace, margins.bottom());
}

QTEST_MAIN(tst_QQuickTableView)

#include "tst_qquicktableview.moc"
