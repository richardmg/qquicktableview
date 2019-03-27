/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

import QtQuick 2.12
import QtQuick.Window 2.3
import QtQml.Models 2.2
import TestTableModel 0.1
import QtQuick.Controls 2.5

Window {
    id: window
    width: 640
    height: 480
    visible: true

    property int selectedX: -1
    property int selectedY: -1

    TestTableModel {
        id: tableModel
        rowCount: 200
        columnCount: 200
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 10
        color: "darkgray"

        Row {
            id: menu
            x: 2
            y: 2
            spacing: 1
//            Button {
//                text: "Add row"
//                onClicked: tableModel.insertRows(selectedY, 1)
//            }
//            Button {
//                text: "Remove row"
//                onClicked: tableModel.removeRows(selectedY, 1)
//            }
//            Button {
//                text: "Add column"
//                onClicked: tableModel.insertColumns(selectedX, 1)
//            }
//            Button {
//                text: "Remove column"
//                onClicked: tableModel.removeColumns(selectedX, 1)
//            }
            Button {
                text: "headerview jmp"
                onClicked: headerView.contentX += 1000
            }
            Button {
                text: "tableview jmp"
                onClicked: tableView.contentX += 1000
            }
        }
        Text {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 2
            text: "x:" + selectedX + ", y:" + selectedY
        }

        TableView {
            id: headerView
            objectName: "headerview"
            anchors.left: parent.left
            anchors.top: menu.bottom
            height: 100
            width: 400
            anchors.margins: 2
            clip: true

            model: TestTableModel {
                rowCount: 200
                columnCount: 10
            }
            delegate: tableViewDelegate
            columnSpacing: 10
            rowSpacing: 10

            masterView: tableView
            onContentXChanged: print(contentX)
        }

        TableView {
            id: tableView
            objectName: "tableview"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: headerView.bottom
            anchors.bottom: parent.bottom
            width: 200
            anchors.margins: 2
            clip: true
            columnWidthProvider: function(c) { return 100 + c; }
            ScrollBar.horizontal: ScrollBar {}

            model: TestTableModel {
                rowCount: 200
                columnCount: 200
            }
            delegate: tableViewDelegate
            columnSpacing: 10
            rowSpacing: 10
//            contentWidth: 61193
//            onContentWidthChanged: print(contentWidth)
        }

        Component.onCompleted: {
            tableView.contentX = 900//1200
            tableView.forceLayout()
            headerView.masterView = null
//            tableView.contentX = 900


        }

        Component {
            id: tableViewDelegate
            Rectangle {
                id: delegate
                implicitWidth: 100
                implicitHeight: 50
                color: display
                border.width: row === selectedY && column == selectedX ? 2 : 0
                border.color: "darkgreen"

                Text {
                    anchors.fill: parent
                    text: column + ", " + row

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            selectedX = column
                            selectedY = row
                        }
                    }
                }
            }
        }

    }

}
