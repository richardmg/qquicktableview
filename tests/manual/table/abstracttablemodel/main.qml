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

Window {
    id: window
    width: 640
    height: 480
    visible: true

    TestTableModel {
        id: tableModel
        rowCount: 20
        columnCount: 20
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
                model: tableModel
                delegate: tableDelegate
                cacheBuffer: 500
                columns: 100
                rows: 100
                columnSpacing: 1
                rowSpacing: 1
            }
        }

        Text {
            font.pixelSize: 8
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.margins: 2
            horizontalAlignment: Qt.AlignRight
            text: qsTr("rows=%1\ncolumns=%2").arg(table.rows).arg(table.columns)
        }

        Component {
            id: tableDelegate
            Rectangle {
                width: column % 3 ? 80 : 50
                height: row % 3 ? 80 : 50
                color: model.display ? "white" : Qt.rgba(0.96, 0.96, 0.96, 1)

                Text {
                    anchors.centerIn: parent
                    text: model.display ? model.display : ""
                }
            }
        }

    }

}