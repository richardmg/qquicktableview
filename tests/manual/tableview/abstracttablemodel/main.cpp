#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QAbstractTableModel>

class TableModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(int rowCount READ rowCount WRITE setRowCount NOTIFY rowCountChanged)
    Q_PROPERTY(int columnCount READ columnCount WRITE setColumnCount NOTIFY columnCountChanged)

public:
    TableModel(QObject *parent = nullptr) : QAbstractTableModel(parent) { }

    int rowCount(const QModelIndex & = QModelIndex()) const override { return m_rows; }
    void setRowCount(int count) { beginResetModel(); m_rows = count; emit rowCountChanged(); endResetModel(); }

    int columnCount(const QModelIndex & = QModelIndex()) const override { return m_cols; }
    void setColumnCount(int count) { beginResetModel(); m_cols = count; emit columnCountChanged(); endResetModel(); }

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
        return QString("[%1-%2]").arg(index.row()).arg(index.column());
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return { {Qt::DisplayRole, "display"} };
    }

signals:
    void rowCountChanged();
    void columnCountChanged();

private:
    int m_rows = 0;
    int m_cols = 0;
};

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    qmlRegisterType<TableModel>("QAbstractTableModel", 0, 1, "QAbstractTableModel");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    return app.exec();
}

#include "main.moc"
