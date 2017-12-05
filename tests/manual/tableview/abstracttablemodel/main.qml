import QtQuick 2.10
import QtQuick.Window 2.3
import QtQml.Models 2.2
import QAbstractTableModel 0.1

Window {
    id: window
    width: 640
    height: 480
    visible: true

    QAbstractTableModel {
        id: tableModel
        rowCount: 20
        columnCount: 20
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 10
        color: "darkgray"

        Item {
            anchors.fill: parent
            anchors.margins: 1

//            TableView {
//                id: tableHeader
//                width: parent.width
//                height: tableView.rowHeight(0)//30
//                model: tableView.columns
//                clip: true
//                columnSpacing: tableView.columnSpacing

//                interactive: false
//                contentX: tableView.contentX

//                columns: tableView.columns
//                rows: 1

//                delegate: Rectangle {
//                    color: "lightgreen"
//                    width: tableView.columnWidth(index)
//                    height: tableHeader.height

//                    Text {
//                        anchors.centerIn: parent
//                        text: qsTr("column %1").arg(index);
//                    }
//                }
//            }

            TableView {
                id: tableView
                clip: true
                anchors.topMargin: 20//tableHeader.height + rowSpacing
                anchors.fill: parent

                cacheBuffer: 0
                model: tableModel

                // Setting columns and rows is not really needed, since the model contains the info.
                // But it should probably be the opposite, so that we always use columns and rows?
                // OTHOH, should we support ListModel, or only TableModel (and what should TableModel look like?)?
                columns: 1000
                rows: 50

                columnSpacing: 1
                rowSpacing: 1

                delegate: cellComponent
            }
        }

        Text {
            font.pixelSize: 8
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.margins: 2
            horizontalAlignment: Qt.AlignRight
            text: qsTr("count=%1\nrows=%2\ncolumns=%3").arg(tableView.count).arg(tableView.rows).arg(tableView.columns)
        }

        Component {
            id: cellComponent
            Rectangle {
                width: 120
                height: 60
                color: model.display ? "white" : Qt.rgba(0.96, 0.96, 0.96, 1)

                Text {
                    anchors.centerIn: parent
                    text: model.display ? model.display : ""
                }

//                Component.onCompleted: if (column === 0) wait(500)
            }
        }

    }

    function wait(ms){
        var start = new Date().getTime();
        var end = start;
        while(end < start + ms)
            end = new Date().getTime();
    }

}
