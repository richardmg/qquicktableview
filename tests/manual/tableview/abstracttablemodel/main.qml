import QtQuick 2.10
import QtQuick.Window 2.3
import QtQml.Models 2.2
import QAbstractTableModel 0.1

Window {
    id: window
    width: 640
    height: 480
    visible: true

    ListModel {

        id: listModel
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

    Rectangle {
        anchors.fill: parent
        anchors.margins: 10
        color: "darkgray"

        TableView {
            id: listModelView
            clip: true
            anchors.fill: parent

            //contentHeight: 1000;

            cacheBuffer: 0
            model: listModel

            // Setting columns and rows is not really needed, since the ListModel contains the info.
            // But it should probably be the oposite, so that we always use columns and rows?
            // OTHOH, should we support ListModel, or only TableModel (and what should TableModel look like?)?
            columns: 200
            rows: 500

            columnSpacing: 1
            rowSpacing: 1

            delegate: Rectangle {
                width: 120
                height: 60
                color: foo ? "white" : Qt.rgba(0.96, 0.96, 0.96, 1)
                Text {
                    x: 2
                    y: 2
                    text: "[" + model.row + ", " + column + "]\nIndex: " + index + (foo ? "\nModel: " + foo : "")
                }
            }

            Text {
                font.pixelSize: 8
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                horizontalAlignment: Qt.AlignRight
                text: qsTr("count=%1\nrows=%2\ncolumns=%3").arg(listModelView.count).arg(listModelView.rows).arg(listModelView.columns)
            }
        }
    }
}
