/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef QQUICKABSTRACTITEMVIEW_P_H
#define QQUICKABSTRACTITEMVIEW_P_H

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

QT_REQUIRE_CONFIG(quick_itemview);

#include "qquickflickable_p.h"
#include <qpointer.h>
#include <QtCore/QLoggingCategory>

QT_BEGIN_NAMESPACE

class QQuickAbstractItemViewPrivate;

class Q_QUICK_PRIVATE_EXPORT QQuickAbstractItemView : public QQuickFlickable
{
    Q_OBJECT

    Q_PROPERTY(QQuickTransition *populate READ populateTransition WRITE setPopulateTransition NOTIFY populateTransitionChanged)
    Q_PROPERTY(QQuickTransition *add READ addTransition WRITE setAddTransition NOTIFY addTransitionChanged)
    Q_PROPERTY(QQuickTransition *addDisplaced READ addDisplacedTransition WRITE setAddDisplacedTransition NOTIFY addDisplacedTransitionChanged)
    Q_PROPERTY(QQuickTransition *move READ moveTransition WRITE setMoveTransition NOTIFY moveTransitionChanged)
    Q_PROPERTY(QQuickTransition *moveDisplaced READ moveDisplacedTransition WRITE setMoveDisplacedTransition NOTIFY moveDisplacedTransitionChanged)
    Q_PROPERTY(QQuickTransition *remove READ removeTransition WRITE setRemoveTransition NOTIFY removeTransitionChanged)
    Q_PROPERTY(QQuickTransition *removeDisplaced READ removeDisplacedTransition WRITE setRemoveDisplacedTransition NOTIFY removeDisplacedTransitionChanged)
    Q_PROPERTY(QQuickTransition *displaced READ displacedTransition WRITE setDisplacedTransition NOTIFY displacedTransitionChanged)

public:
    ~QQuickAbstractItemView();

    QQuickTransition *populateTransition() const;
    void setPopulateTransition(QQuickTransition *transition);

    QQuickTransition *addTransition() const;
    void setAddTransition(QQuickTransition *transition);

    QQuickTransition *addDisplacedTransition() const;
    void setAddDisplacedTransition(QQuickTransition *transition);

    QQuickTransition *moveTransition() const;
    void setMoveTransition(QQuickTransition *transition);

    QQuickTransition *moveDisplacedTransition() const;
    void setMoveDisplacedTransition(QQuickTransition *transition);

    QQuickTransition *removeTransition() const;
    void setRemoveTransition(QQuickTransition *transition);

    QQuickTransition *removeDisplacedTransition() const;
    void setRemoveDisplacedTransition(QQuickTransition *transition);

    QQuickTransition *displacedTransition() const;
    void setDisplacedTransition(QQuickTransition *transition);

Q_SIGNALS:
    void populateTransitionChanged();
    void addTransitionChanged();
    void addDisplacedTransitionChanged();
    void moveTransitionChanged();
    void moveDisplacedTransitionChanged();
    void removeTransitionChanged();
    void removeDisplacedTransitionChanged();
    void displacedTransitionChanged();

protected:
    QQuickAbstractItemView(QQuickFlickablePrivate &dd, QQuickItem *parent = nullptr);

private:
    Q_DECLARE_PRIVATE(QQuickAbstractItemView)
};

QT_END_NAMESPACE

#endif // QQUICKABSTRACTITEMVIEW_P_H

