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




//        TableView {
//            id: tableModelView
//            model: QAbstractTableModel {
//                rowCount: 10
//                columnCount: 10
//            }
//            delegate: Text {
//                text: modelData
//                width: tableModelView.cellWidth
//                height: tableModelView.cellHeight
////                Text {
////                    font.pixelSize: 8
////                    anchors.bottom: parent.bottom
////                    anchors.right: parent.right
////                    text: qsTr("index=%1\nrow=%2\ncolumn=%3").arg(index).arg(row).arg(column)
////                }
//            }
//            Text { font.pixelSize: 8; text: "QAbstractTableModel (model columns)" }
//            Text {
//                font.pixelSize: 8
//                anchors.bottom: parent.bottom
//                anchors.right: parent.right
//                horizontalAlignment: Qt.AlignRight
//                text: qsTr("count=%1\nrows=%2\ncolumns=%3").arg(tableModelView.count).arg(tableModelView.rows).arg(tableModelView.columns)
//            }
//            clip: true
//            width: parent.width
//            height: parent.height / 3 - 2
//        }





        Rectangle { width: parent.width; height: 1; color: "silver" }

        TableView {
            id: listModelView
//            width: parent.width
//            height: parent.height
            clip: true
            width: parent.width
            height: parent.height / 3 - 2

            // Setting columns and rows is not really needed, since the ListModel contains the info.
            // OTHOH, should we support ListModel, or only TableModel (and what should TableModel look like?)?
            columns: 2

            model: ListModel {
                id: myModel
                ListElement { foo: "foo0"; bar: "bar0" }
                ListElement { foo: "foo1"; bar: "bar1" }
                ListElement { foo: "foo2"; bar: "bar2" }
                ListElement { foo: "foo3"; bar: "bar0" }
                ListElement { foo: "foo4"; bar: "bar0" }
                ListElement { foo: "foo5"; bar: "bar0" }
                ListElement { foo: "foo6"; bar: "bar1" }
                ListElement { foo: "foo7"; bar: "bar2" }
                ListElement { foo: "foo8"; bar: "bar1" }
                ListElement { foo: "foo9"; bar: "bar2" }
                ListElement { foo: "foo10"; bar: "bar1" }
                ListElement { foo: "foo11"; bar: "bar1" }
                ListElement { foo: "foo12"; bar: "bar1" }
                ListElement { foo: "foo13"; bar: "bar1" }
                ListElement { foo: "foo14"; bar: "bar1" }
                ListElement { foo: "foo15"; bar: "bar1" }
                ListElement { foo: "foo16"; bar: "bar1" }
                ListElement { foo: "foo17"; bar: "bar1" }
                ListElement { foo: "foo18"; bar: "bar1" }
                ListElement { foo: "foo19"; bar: "bar1" }
                ListElement { foo: "foo20"; bar: "bar1" }
                ListElement { foo: "foo21"; bar: "bar1" }
                ListElement { foo: "foo22"; bar: "bar1" }
                ListElement { foo: "foo23"; bar: "bar1" }
                ListElement { foo: "foo24"; bar: "bar1" }
                ListElement { foo: "foo25"; bar: "bar1" }
                ListElement { foo: "foo26"; bar: "bar1" }
                ListElement { foo: "foo27"; bar: "bar1" }
                ListElement { foo: "foo28"; bar: "bar1" }
                ListElement { foo: "foo29"; bar: "bar1" }
//                Component.onCompleted: {
//                    for (var i = 0; i < 100; ++i)
//                        myModel.append({ foo: "foo" + i, bar: "bar" + i })
//                }
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
        }




//        Rectangle { width: parent.width; height: 1; color: "silver" }

//        TableView {
//            id: intModelView
//            rows: 100000
//            columns: 1000
//            model: 100000000
//            delegate: Text {
//                text: modelData
//            }
//            Text { font.pixelSize: 8; text: "Integer" }
//            Text {
//                font.pixelSize: 8
//                anchors.bottom: parent.bottom
//                anchors.right: parent.right
//                horizontalAlignment: Qt.AlignRight
//                text: qsTr("count=%1\nrows=%2\ncolumns=%3").arg(intModelView.count).arg(intModelView.rows).arg(intModelView.columns)
//            }
//            clip: true
//            width: parent.width
//            height: parent.height / 3 - 2
//        }

    }
}
