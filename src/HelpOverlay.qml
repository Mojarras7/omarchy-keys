import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: overlay
    anchors.fill: parent
    color: "#cc09090b"
    z: 100

    signal closeRequested()

    TapHandler { onTapped: overlay.closeRequested() }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 48)
        implicitHeight: cardLayout.implicitHeight + 36
        color: "#18181d"
        border.color: "#2e2e38"
        border.width: 1
        radius: theme.radius

        TapHandler {} // Prevent closing when clicking inside card.

        ColumnLayout {
            id: cardLayout
            anchors.fill: parent
            anchors.margins: 18
            spacing: 10

            Label {
                text: qsTr("Keyboard shortcuts")
                font.pixelSize: 17
                font.weight: Font.DemiBold
                color: "#f4f4f6"
            }

            Repeater {
                model: [
                    { key: "1 / 2 / 3", desc: qsTr("Switch tabs") },
                    { key: "/", desc: qsTr("Search all bindings") },
                    { key: "N", desc: qsTr("Add new shortcut") },
                    { key: "Space", desc: qsTr("Refresh usage scan") },
                    { key: "Ctrl+Z / Ctrl+Shift+Z", desc: qsTr("Undo / Redo") },
                    { key: "?", desc: qsTr("Toggle this help") },
                    { key: "Q", desc: qsTr("Quit") }
                ]
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 12

                    Rectangle {
                        color: "#121216"
                        border.color: "#2e2e38"
                        border.width: 1
                        radius: Math.min(4, theme.radius)
                        implicitWidth: 150
                        implicitHeight: 24
                        Label {
                            anchors.centerIn: parent
                            text: modelData.key
                            font.pixelSize: 11
                            font.family: "monospace"
                            color: theme.accent
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: modelData.desc
                        font.pixelSize: 13
                        color: "#d4d4dc"
                    }
                }
            }
        }
    }
}
