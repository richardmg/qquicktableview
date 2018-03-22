/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QQMLDATAMODEL_P_H
#define QQMLDATAMODEL_P_H

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

#include <private/qtqmlglobal_p.h>
#include <private/qqmllistcompositor_p.h>
#include <private/qqmlobjectmodel_p.h>
#include <private/qqmlincubator_p.h>

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qstringlist.h>

#include <private/qv8engine_p.h>
#include <private/qqmlglobal_p.h>

QT_REQUIRE_CONFIG(qml_delegate_model);

QT_BEGIN_NAMESPACE

class QQmlChangeSet;
class QQmlComponent;
class QQuickPackage;
class QQmlV4Function;
class QQmlDelegateModelGroup;
class QQmlDelegateModelAttached;
class QQmlDelegateModelPrivate;
class QQmlDelegateChooser;


class Q_QML_PRIVATE_EXPORT QQmlDelegateModel : public QQmlInstanceModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQmlDelegateModel)

    Q_PROPERTY(QVariant model READ model WRITE setModel)
    Q_PROPERTY(QQmlComponent *delegate READ delegate WRITE setDelegate)
    Q_PROPERTY(QQmlDelegateChooser *delegateChooser READ delegateChooser WRITE setDelegateChooser REVISION 11)
    Q_PROPERTY(QString filterOnGroup READ filterGroup WRITE setFilterGroup NOTIFY filterGroupChanged RESET resetFilterGroup)
    Q_PROPERTY(QQmlDelegateModelGroup *items READ items CONSTANT) //TODO : worth renaming?
    Q_PROPERTY(QQmlDelegateModelGroup *persistedItems READ persistedItems CONSTANT)
    Q_PROPERTY(QQmlListProperty<QQmlDelegateModelGroup> groups READ groups CONSTANT)
    Q_PROPERTY(QObject *parts READ parts CONSTANT)
    Q_PROPERTY(QVariant rootIndex READ rootIndex WRITE setRootIndex NOTIFY rootIndexChanged)
    Q_PROPERTY(int rows READ rows NOTIFY rowsChanged REVISION 12)
    Q_PROPERTY(int columns READ columns NOTIFY columnsChanged REVISION 12)
    Q_CLASSINFO("DefaultProperty", "delegate")
    Q_INTERFACES(QQmlParserStatus)
public:
    QQmlDelegateModel();
    QQmlDelegateModel(QQmlContext *, QObject *parent=nullptr);
    ~QQmlDelegateModel();

    void classBegin() override;
    void componentComplete() override;

    QVariant model() const;
    void setModel(const QVariant &);

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *);

    QQmlDelegateChooser *delegateChooser() const;
    void setDelegateChooser(QQmlDelegateChooser *chooser);

    QVariant rootIndex() const;
    void setRootIndex(const QVariant &root);

    int rows() const;
    int columns() const;

    Q_INVOKABLE QVariant modelIndex(int idx) const;
    Q_INVOKABLE QVariant parentModelIndex() const;

    int count() const override;
    bool isValid() const override { return delegate() != nullptr; }
    QObject *object(int index, QQmlIncubator::IncubationMode incubationMode = QQmlIncubator::AsynchronousIfNested) override;
    ReleaseFlags release(QObject *object, bool recyclable);
    ReleaseFlags release(QObject *object) override { return release(object, false); }
    void cancel(int index) override;
    QString stringValue(int index, const QString &role) override;
    void setWatchedRoles(const QList<QByteArray> &roles) override;
    QQmlIncubator::Status incubationStatus(int index) override;

    int indexOf(QObject *object, QObject *objectContext) const override;

    QString filterGroup() const;
    void setFilterGroup(const QString &group);
    void resetFilterGroup();

    QQmlDelegateModelGroup *items();
    QQmlDelegateModelGroup *persistedItems();
    QQmlListProperty<QQmlDelegateModelGroup> groups();
    QObject *parts();

    int recyclePoolMaxSize();
    void setRecyclePoolMaxSize(int maxSize);

    bool event(QEvent *) override;

    static QQmlDelegateModelAttached *qmlAttachedProperties(QObject *obj);

Q_SIGNALS:
    void filterGroupChanged();
    void defaultGroupsChanged();
    void rootIndexChanged();
    void initRecycledItem(int index, QObject *object);
    Q_REVISION(12) void rowsChanged();
    Q_REVISION(12) void columnsChanged();

private Q_SLOTS:
    void _q_itemsChanged(int index, int count, const QVector<int> &roles);
    void _q_itemsInserted(int index, int count);
    void _q_itemsRemoved(int index, int count);
    void _q_itemsMoved(int from, int to, int count);
    void _q_modelReset();
    void _q_rowsInserted(const QModelIndex &,int,int);
    void _q_rowsAboutToBeRemoved(const QModelIndex &parent, int begin, int end);
    void _q_rowsRemoved(const QModelIndex &,int,int);
    void _q_rowsMoved(const QModelIndex &, int, int, const QModelIndex &, int);
    void _q_dataChanged(const QModelIndex&,const QModelIndex&,const QVector<int> &);
    void _q_layoutChanged(const QList<QPersistentModelIndex>&, QAbstractItemModel::LayoutChangeHint);

private:
    bool isDescendantOf(const QPersistentModelIndex &desc, const QList<QPersistentModelIndex> &parents) const;

    Q_DISABLE_COPY(QQmlDelegateModel)
};

class QQmlDelegateModelGroupPrivate;
class Q_QML_PRIVATE_EXPORT QQmlDelegateModelGroup : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool includeByDefault READ defaultInclude WRITE setDefaultInclude NOTIFY defaultIncludeChanged)
public:
    QQmlDelegateModelGroup(QObject *parent = nullptr);
    QQmlDelegateModelGroup(const QString &name, QQmlDelegateModel *model, int compositorType, QObject *parent = nullptr);
    ~QQmlDelegateModelGroup();

    QString name() const;
    void setName(const QString &name);

    int count() const;

    bool defaultInclude() const;
    void setDefaultInclude(bool include);

    Q_INVOKABLE QQmlV4Handle get(int index);

public Q_SLOTS:
    void insert(QQmlV4Function *);
    void create(QQmlV4Function *);
    void resolve(QQmlV4Function *);
    void remove(QQmlV4Function *);
    void addGroups(QQmlV4Function *);
    void removeGroups(QQmlV4Function *);
    void setGroups(QQmlV4Function *);
    void move(QQmlV4Function *);

Q_SIGNALS:
    void countChanged();
    void nameChanged();
    void defaultIncludeChanged();
    void changed(const QQmlV4Handle &removed, const QQmlV4Handle &inserted);
private:
    Q_DECLARE_PRIVATE(QQmlDelegateModelGroup)
};

class QQmlDelegateModelItem;
class QQmlDelegateModelAttachedMetaObject;
class QQmlDelegateModelAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlDelegateModel *model READ model CONSTANT)
    Q_PROPERTY(QStringList groups READ groups WRITE setGroups NOTIFY groupsChanged)
    Q_PROPERTY(bool isUnresolved READ isUnresolved NOTIFY unresolvedChanged)
public:
    QQmlDelegateModelAttached(QObject *parent);
    QQmlDelegateModelAttached(QQmlDelegateModelItem *cacheItem, QObject *parent);
    ~QQmlDelegateModelAttached() {}

    void setCacheItem(QQmlDelegateModelItem *item);

    QQmlDelegateModel *model() const;

    QStringList groups() const;
    void setGroups(const QStringList &groups);

    bool isUnresolved() const;

    void emitChanges();

    void emitUnresolvedChanged() { Q_EMIT unresolvedChanged(); }

Q_SIGNALS:
    void groupsChanged();
    void unresolvedChanged();

public:
    QQmlDelegateModelItem *m_cacheItem;
    int m_previousGroups;
    int m_currentIndex[QQmlListCompositor::MaximumGroupCount];
    int m_previousIndex[QQmlListCompositor::MaximumGroupCount];

    friend class QQmlDelegateModelAttachedMetaObject;
};

// TODO: consider making QQmlDelegateChooser public API
class QQmlDelegateChooserPrivate;
class Q_QML_PRIVATE_EXPORT QQmlDelegateChooser : public QObject
{
    Q_OBJECT
public:
    QQmlDelegateChooser(QObject *parent = nullptr);
    ~QQmlDelegateChooser() override;

    virtual QQmlComponent *delegate(int index) const = 0;

protected:
    QVariant value(int index, const QString &role) const;
    void delegateChange() const;

private:
    Q_DECLARE_PRIVATE(QQmlDelegateChooser)
    Q_DISABLE_COPY(QQmlDelegateChooser)
};

class Q_QML_PRIVATE_EXPORT QQmlDelegateChoice : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)
    Q_PROPERTY(int startIndex READ startIndex WRITE setStartIndex NOTIFY startIndexChanged)
    Q_PROPERTY(int endIndex READ endIndex WRITE setEndIndex NOTIFY endIndexChanged)
    Q_PROPERTY(int nthIndex READ nthIndex WRITE setNthIndex NOTIFY nthIndexChanged)
    Q_CLASSINFO("DefaultProperty", "delegate")
public:
    QVariant value() const { return m_value; }
    void setValue(const QVariant &value);

    QQmlComponent *delegate() const { return m_delegate; }
    void setDelegate(QQmlComponent *delegate);

    int startIndex() const { return m_startIndex; }
    void setStartIndex(int index);

    int endIndex() const { return m_endIndex; }
    void setEndIndex(int index);

    int nthIndex() const { return m_nthIndex; }
    void setNthIndex(int index);

    bool match(int index, const QVariant &value) const;

signals:
    void valueChanged();
    void delegateChanged();
    void startIndexChanged();
    void endIndexChanged();
    void nthIndexChanged();
    void changed();

private:
    int m_startIndex = -1;
    int m_endIndex = -1;
    int m_nthIndex = -1;
    QVariant m_value;
    QQmlComponent *m_delegate = nullptr;
};

class Q_QML_PRIVATE_EXPORT QQmlDefaultDelegateChooser : public QQmlDelegateChooser
{
    Q_OBJECT
    Q_PROPERTY(QString role READ role WRITE setRole NOTIFY roleChanged)
    Q_PROPERTY(QQmlListProperty<QQmlDelegateChoice> choices READ choices CONSTANT)
    Q_CLASSINFO("DefaultProperty", "choices")

public:
    QString role() const { return m_role; }
    void setRole(const QString &role);

    QQmlListProperty<QQmlDelegateChoice> choices();
    static void choices_append(QQmlListProperty<QQmlDelegateChoice> *, QQmlDelegateChoice *);
    static int choices_count(QQmlListProperty<QQmlDelegateChoice> *);
    static QQmlDelegateChoice *choices_at(QQmlListProperty<QQmlDelegateChoice> *, int);
    static void choices_clear(QQmlListProperty<QQmlDelegateChoice> *);

    QQmlComponent *delegate(int index) const override;

signals:
    void roleChanged();

private:
    QString m_role;
    QList<QQmlDelegateChoice*> m_choices;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQmlDelegateModel)
QML_DECLARE_TYPEINFO(QQmlDelegateModel, QML_HAS_ATTACHED_PROPERTIES)
QML_DECLARE_TYPE(QQmlDelegateModelGroup)
QML_DECLARE_TYPE(QQmlDelegateChoice)
QML_DECLARE_TYPE(QQmlDefaultDelegateChooser)

#endif // QQMLDATAMODEL_P_H
