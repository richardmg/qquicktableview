/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQMLTABLEINSTANCEMODEL_P_H
#define QQMLTABLEINSTANCEMODEL_P_H

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

#include <QtQml/private/qqmldelegatemodel_p.h>
#include <QtQml/private/qqmldelegatemodel_p_p.h>

QT_BEGIN_NAMESPACE

class QQmlTableInstanceModel;

class QQmlTableInstanceModelIncubationTask : public QQDMIncubationTask
{
public:
    QQmlTableInstanceModelIncubationTask(QQmlTableInstanceModel *tim, QQmlDelegateModelItem* modelItemToIncubate, IncubationMode mode)
        : QQDMIncubationTask(nullptr, mode)
        , modelItemToIncubate(modelItemToIncubate)
        , tableInstanceModel(tim) {
        clear();
    }

    void statusChanged(Status status) override;
    void setInitialState(QObject *object) override;

    QQmlDelegateModelItem *modelItemToIncubate = nullptr;
    QQmlTableInstanceModel *tableInstanceModel = nullptr;
};

class Q_QML_PRIVATE_EXPORT QQmlTableInstanceModel : public QQmlInstanceModel
{
public:
    QQmlTableInstanceModel(QQmlContext *qmlContext, QObject *parent = nullptr);
    ~QQmlTableInstanceModel() override;

    bool event(QEvent *e) override;

    int count() const override { return m_adaptorModel.count(); }
    int rows() const { return m_adaptorModel.rowCount(); }
    int columns() const { return m_adaptorModel.columnCount(); }

    bool isValid() const override { return true; }

    QVariant model() const;
    void setModel(const QVariant &model);

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *);

    QObject *object(int index, QQmlIncubator::IncubationMode incubationMode = QQmlIncubator::AsynchronousIfNested) override;
    ReleaseFlags release(QObject *) override;
    void cancel(int) override;

    QQmlIncubator::Status incubationStatus(int index) override;

    QString stringValue(int, const QString &) override { Q_UNREACHABLE(); return QString(); }
    void setWatchedRoles(const QList<QByteArray> &) override { Q_UNREACHABLE(); }
    int indexOf(QObject *, QObject *) const override { Q_UNREACHABLE(); return 0; }

private:
    QQmlComponent *m_delegate = nullptr;
    QQmlAdaptorModel m_adaptorModel;
    QPointer<QQmlContext> m_qmlContext;
    QQmlDelegateModelItemMetaType *m_metaType;

    QHash<int, QQmlDelegateModelItem *> m_modelItems;
    QList<QQmlIncubator *> m_finishedIncubationTasks;

    void incubatorStatusChanged(QQmlTableInstanceModelIncubationTask *dmIncubationTask, QQmlIncubator::Status status);
    void deleteIncubationTaskLater(QQmlIncubator *incubationTask);
    void deleteAllPostedIncubationTasks();

    void connectToAbstractItemModel();
    void disconnectToAbstractItemModel();

    void dataChangedCallback(const QModelIndex &begin, const QModelIndex &end, const QVector<int> &roles);

    static bool isDoneIncubating(QQmlDelegateModelItem *modelItem);
    static void deleteModelItemLater(QQmlDelegateModelItem *modelItem);

    friend class QQmlTableInstanceModelIncubationTask;
};

QT_END_NAMESPACE

#endif // QQMLTABLEINSTANCEMODEL_P_H
