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

#ifndef QQUICKTABLEVIEW_P_H
#define QQUICKTABLEVIEW_P_H

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

#include <QtQuick/private/qtquickglobal_p.h>

QT_REQUIRE_CONFIG(quick_tableview);

#include "qquickabstractitemview_p.h"

QT_BEGIN_NAMESPACE

class QQuickTableViewAttached;
class QQuickTableViewPrivate;

class Q_AUTOTEST_EXPORT QQuickTableView : public QQuickAbstractItemView
{
    Q_OBJECT

    Q_PROPERTY(int rows READ rows WRITE setRows RESET resetRows NOTIFY rowsChanged)
    Q_PROPERTY(int columns READ columns WRITE setColumns RESET resetColumns NOTIFY columnsChanged)
    Q_PROPERTY(qreal rowSpacing READ rowSpacing WRITE setRowSpacing NOTIFY rowSpacingChanged)
    Q_PROPERTY(qreal columnSpacing READ columnSpacing WRITE setColumnSpacing NOTIFY columnSpacingChanged)

    Q_CLASSINFO("DefaultProperty", "data")

public:
    QQuickTableView(QQuickItem *parent = nullptr);

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

    static QQuickTableViewAttached *qmlAttachedProperties(QObject *);

Q_SIGNALS:
    void rowsChanged();
    void columnsChanged();
    void rowSpacingChanged();
    void columnSpacingChanged();

protected Q_SLOTS:
    void initItem(int index, QObject *item) override;

protected:
    void componentComplete() override;

private:
    Q_DISABLE_COPY(QQuickTableView)
    Q_DECLARE_PRIVATE(QQuickTableView)
};

class QQuickTableViewAttached : public QQuickItemViewAttached
{
    Q_OBJECT
public:
    QQuickTableViewAttached(QObject *parent)
        : QQuickItemViewAttached(parent) {}
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickTableView)
QML_DECLARE_TYPEINFO(QQuickTableView, QML_HAS_ATTACHED_PROPERTIES)

#endif // QQUICKTABLEVIEW_P_H
