import QtQuick 2.10
import QtQuick.Window 2.3
import QtQml.Models 2.2
import QAbstractTableModel 0.1

Window {
    id: window
    width: 640
    height: 480
    visible: true

    Column {
        spacing: 1
        anchors.fill: parent

        TableView {
            id: tableModelView
            model: QAbstractTableModel {
                rowCount: 10
                columnCount: 3
            }
            delegate: Text {
                text: modelData
                width: tableModelView.cellWidth
                height: tableModelView.cellHeight
                Text {
                    font.pixelSize: 8
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    text: qsTr("index=%1\nrow=%2\ncolumn=%3").arg(index).arg(row).arg(column)
                }
            }
            Text { font.pixelSize: 8; text: "QAbstractTableModel (model columns)" }
            Text {
                font.pixelSize: 8
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                horizontalAlignment: Qt.AlignRight
                text: qsTr("count=%1\nrows=%2\ncolumns=%3").arg(tableModelView.count).arg(tableModelView.rows).arg(tableModelView.columns)
            }
            clip: true
            width: parent.width
            height: parent.height / 3 - 2
        }

        Rectangle { width: parent.width; height: 1; color: "silver" }

        TableView {
            id: listModelView
            columns: 2
            model: ListModel {
                ListElement { foo: "foo0"; bar: "bar0" }
                ListElement { foo: "foo1"; bar: "bar1" }
                ListElement { foo: "foo2"; bar: "bar2" }
            }
            delegate: Text {
                text: column == 0 ? foo : bar // modelData
            }
            Text { font.pixelSize: 8; text: "QML ListModel (roles)" }
            Text {
                font.pixelSize: 8
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                horizontalAlignment: Qt.AlignRight
                text: qsTr("count=%1\nrows=%2\ncolumns=%3").arg(listModelView.count).arg(listModelView.rows).arg(listModelView.columns)
            }
            clip: true
            width: parent.width
            height: parent.height / 3 - 2
        }

        Rectangle { width: parent.width; height: 1; color: "silver" }

        TableView {
            id: intModelView
            rows: 100000
            columns: 1000
            model: 100000000
            delegate: Text {
                text: modelData
            }
            Text { font.pixelSize: 8; text: "Integer" }
            Text {
                font.pixelSize: 8
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                horizontalAlignment: Qt.AlignRight
                text: qsTr("count=%1\nrows=%2\ncolumns=%3").arg(intModelView.count).arg(intModelView.rows).arg(intModelView.columns)
            }
            clip: true
            width: parent.width
            height: parent.height / 3 - 2
        }
    }
}
