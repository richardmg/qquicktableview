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
import QtQuick.Controls 1.4

Window {
    id: window
    width: 640
    height: 480
    visible: true

    ListModel {
        id: listModel

        ListElement { name: "name 0" }
        ListElement { name: "name 1" }
        ListElement { name: "name 2" }
        ListElement { name: "name 3" }
        ListElement { name: "name 4" }
        ListElement { name: "name 5" }
        ListElement { name: "name 6" }
        ListElement { name: "name 7" }
        ListElement { name: "name 8" }
        ListElement { name: "name 9" }
        ListElement { name: "name 10" }
        ListElement { name: "name 11" }
        ListElement { name: "name 12" }
        ListElement { name: "name 13" }
        ListElement { name: "name 14" }
        ListElement { name: "name 15" }
        ListElement { name: "name 16" }
        ListElement { name: "name 17" }
        ListElement { name: "name 18" }
        ListElement { name: "name 19" }
        ListElement { name: "name 20" }
        ListElement { name: "name 21" }
        ListElement { name: "name 22" }
        ListElement { name: "name 23" }
        ListElement { name: "name 24" }
        ListElement { name: "name 25" }
        ListElement { name: "name 26" }
        ListElement { name: "name 27" }
        ListElement { name: "name 28" }
        ListElement { name: "name 29" }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 10
        color: "darkgray"

        Flickable {
            id: flickable
            anchors.fill: parent
            anchors.margins: 1
            clip: true

            contentWidth: table.width
            contentHeight: table.height

            Table {
                id: table
                columnSpacing: 1
                rowSpacing: 1
                cacheBuffer: 0

//                rows: 5
//                columns: 2

                model: listModel
//                model: 100
//                model: ObjectModel {
//                    Rectangle { height: 30; width: 80; color: "red" }
//                    Rectangle { height: 40; width: 90; color: "green" }
//                    Rectangle { height: 25; width: 30; color: "blue" }
//                    Rectangle { height: 30; width: 40; color: "yellow" }
//                    Rectangle { height: 30; width: 50; color: "gray" }
//                    Rectangle { height: 55; width: 60; color: "cyan" }
//                    Rectangle { height: 40; width: 30; color: "purple" }
//                    Rectangle { height: 33; width: 50; color: "lightblue" }
//                    Rectangle { height: 20; width: 80; color: "black" }
//                }

                delegate: Rectangle {
                    width: 50//row == 3 ? 120 : 50
                    height: 60 + (5 * row)
                    color: Qt.rgba(0.96, 0.96, 0.96, 1)
                    Text {
                        x: 2
                        y: 2
                        text: modelData//"[" + column + ", " + row + "]\nIndex: " + index + (name ? "\nModel: " + name : "")
                    }
                }
            }

//            Table {
//                id: table2
//                anchors.left: table.right
//                anchors.leftMargin: 20
//                columns: 2
//                columnSpacing: 1
//                rowSpacing: 1

//                model: listModel

//                delegate: Rectangle {
//                    width: 120
//                    height: 60
//                    color: name ? "lightgreen" : Q
//                    Text {
//                        x: 2
//                        y: 2
//                        text: "[" + column + ", " + row + "]\nIndex: " + index + (name ? "\nModel: " + name : "")
//                    }
//                }
//            }


        }
        Button {
            id: button1
            x: 10
            y: 10
            text: "inc spacing"
            onClicked: {
                table.rowSpacing = table.rowSpacing + 2
                table.columnSpacing = table.columnSpacing + 2
            }
        }
        Button {
            x: 10
            anchors.top: button1.bottom
            anchors.margins: 10
            text: "dec spacing"
            onClicked: {
                table.rowSpacing = table.rowSpacing - 2
                table.columnSpacing = table.columnSpacing - 2
            }
        }
    }
}
