/**
 *  OSM
 *  Copyright (C) 2026  Pavel Smokotnin

 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Controls.Material 2.13

import "../" as Root
import "../elements"

Item {
    id: chartProperties
    property var dataObject

    function result(value) {
        return dataObject && dataObject.valid ? Number(value).toFixed(2) : "--";
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 2

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Speech Intelligibility")
                font.bold: true
                Layout.fillWidth: true
            }

            Label {
                text: dataObject ? dataObject.sourceName : ""
                elide: Text.ElideRight
                Layout.maximumWidth: 220
            }

            Label {
                text: qsTr("STI")
            }
            Label {
                text: result(dataObject ? dataObject.sti : 0)
                font.bold: true
            }
            Label {
                text: qsTr("STIPA")
            }
            Label {
                text: result(dataObject ? dataObject.stipa : 0)
                font.bold: true
            }
            Label {
                text: qsTr("RASTI")
            }
            Label {
                text: result(dataObject ? dataObject.rasti : 0)
                font.bold: true
            }

            Button {
                text: qsTr("Refresh")
                onClicked: dataObject.refresh()
                Material.background: parent.Material.background
            }
        }

        Label {
            Layout.fillWidth: true
            text: dataObject ? dataObject.status : ""
            wrapMode: Text.WordWrap
            font.pixelSize: 11
            opacity: 0.8
        }
    }
}
