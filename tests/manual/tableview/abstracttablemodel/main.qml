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
        id: model
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

        ListView {
            id: tableHeader
            width: parent.width
            height: 30
            model: tableView.columns
            clip: true
            orientation: Qt.Horizontal
            spacing: tableView.columnSpacing

            interactive: false
            contentX: tableView.contentX

            delegate: Rectangle {
                color: "lightgreen"
                width: 120
                height: tableHeader.height

                Text {
                    anchors.centerIn: parent
                    text: qsTr("column %1").arg(index);
                }
            }
        }

        TableView {
            id: tableView
            clip: true
            anchors.topMargin: tableHeader.height + rowSpacing
            anchors.fill: parent

            cacheBuffer: 0
            model: model

            // Setting columns and rows is not really needed, since the model contains the info.
            // But it should probably be the opposite, so that we always use columns and rows?
            // OTHOH, should we support ListModel, or only TableModel (and what should TableModel look like?)?
            columns: 100
            rows: 500

            columnSpacing: 1
            rowSpacing: 1

//            Component.onCompleted: horizontalHeader.column(100).width = 50

//            horizontalHeader: TableViewHeader {
//                delegate: Rectangle {
//                    color: "lightgreen"
//                    width: 120
//                    height: 60
//                    Text {
//                        x: 2
//                        y: 2
////                        text: model.headerData.title ? model.headerData.title : ""
//                        text: title ? title : ""
//                    }
//                }

//                Component.onCompleted: {
//                    print("width of column 100:", sectionWidth(100))
//                    print("width of column 100:", column(100).size.width)
//                }
//            }

            delegate: Rectangle {
                width: 120
                height: 60
                color: model.display ? "white" : Qt.rgba(0.96, 0.96, 0.96, 1)

                Text {
                    anchors.centerIn: parent
                    text: model.display ? model.display : ""
                }
            }

            Text {
                font.pixelSize: 8
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                horizontalAlignment: Qt.AlignRight
                text: qsTr("count=%1\nrows=%2\ncolumns=%3").arg(tableView.count).arg(tableView.rows).arg(tableView.columns)
            }
        }
        }
    }
}
