/**
 *  OSM
 *  Copyright (C) 2026  Open Sound Meter contributors

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
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3

Dialog {
    id: dialog

    property var dataObject
    readonly property var curves: dataObject ? dataObject.customCurves : []
    property string errorText: ""

    title: qsTr("Imported weighting curves")
    modal: true
    focus: true
    width: 520
    height: 360
    parent: Overlay.overlay
    x: (parent ? (parent.width - width) / 2 : 0)
    y: (parent ? (parent.height - height) / 2 : 0)
    standardButtons: Dialog.Close
    closePolicy: Popup.CloseOnEscape
    onOpened: errorText = ""

    contentItem: ColumnLayout {
        spacing: 8

        Label {
            Layout.fillWidth: true
            text: qsTr("Rename or remove an imported curve. Built-in A, B, C, and Z curves are fixed.")
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: dialog.curves.length === 0
            text: qsTr("No imported curves.")
        }

        ListView {
            id: curveList

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: dialog.curves.length > 0
            clip: true
            spacing: 6
            model: dialog.curves

            delegate: RowLayout {
                width: curveList.width
                spacing: 6

                TextField {
                    id: nameField

                    Layout.fillWidth: true
                    text: modelData
                    selectByMouse: true
                    onAccepted: renameButton.clicked()
                }

                Button {
                    id: renameButton

                    text: qsTr("rename")
                    onClicked: {
                        var renamed = dialog.dataObject.renameCurve(modelData, nameField.text);
                        dialog.errorText = renamed ? "" : dialog.dataObject.curveError;
                    }
                }

                Button {
                    text: qsTr("remove")
                    onClicked: {
                        if (dialog.dataObject.removeCurve(modelData))
                            dialog.errorText = "";
                        else
                            dialog.errorText = dialog.dataObject.curveError;
                    }
                }

            }

        }

        Label {
            Layout.fillWidth: true
            visible: text !== ""
            text: dialog.errorText
            color: "#d9534f"
            wrapMode: Text.WordWrap
        }

    }

}
