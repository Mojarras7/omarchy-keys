import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: root
    anchors.centerIn: parent
    width: Math.min(500, parent ? parent.width - 40 : 500)
    modal: true
    title: qsTr("Bind shortcut")
    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property string comboValue: ""
    property string descriptionValue: ""
    property string commandValue: ""
    property bool launchMode: true
    property bool recording: false

    function openFor(combo, description, command, isLaunch) {
        comboValue = combo || ""
        descriptionValue = description || ""
        commandValue = command || ""
        launchMode = (isLaunch !== undefined) ? isLaunch : true
        recording = false
        open()
    }

    function keyToName(key) {
        if (key >= Qt.Key_A && key <= Qt.Key_Z)
            return String.fromCharCode(key)
        if (key >= Qt.Key_0 && key <= Qt.Key_9)
            return String.fromCharCode(key)
        if (key >= Qt.Key_F1 && key <= Qt.Key_F12)
            return "F" + (key - Qt.Key_F1 + 1)
        switch (key) {
        case Qt.Key_Return: case Qt.Key_Enter: return "RETURN"
        case Qt.Key_Space: return "SPACE"
        case Qt.Key_Tab: return "TAB"
        case Qt.Key_Backspace: return "BACKSPACE"
        case Qt.Key_Delete: return "DELETE"
        case Qt.Key_Slash: return "SLASH"
        case Qt.Key_Period: return "PERIOD"
        case Qt.Key_Comma: return "COMMA"
        case Qt.Key_Minus: return "MINUS"
        case Qt.Key_Equal: return "EQUAL"
        case Qt.Key_Left: return "LEFT"
        case Qt.Key_Right: return "RIGHT"
        case Qt.Key_Up: return "UP"
        case Qt.Key_Down: return "DOWN"
        default: return ""
        }
    }

    background: Rectangle {
        color: "#18181d"
        border.color: "#2e2e38"
        border.width: 1
        radius: theme.radius
    }

    ColumnLayout {
        width: parent.width
        spacing: 14

        // Key recorder box
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            TextField {
                id: comboField
                Layout.fillWidth: true
                text: root.comboValue
                placeholderText: qsTr("Shortcut (e.g. SUPER + SHIFT + Z)")
                font.family: "monospace"
                Material.roundedScale: theme.radius
                onTextChanged: root.comboValue = text
            }

            Button {
                id: recordBtn
                text: root.recording ? qsTr("Press keys…") : qsTr("Record")
                highlighted: root.recording
                Material.roundedScale: theme.radius
                onClicked: {
                    root.recording = !root.recording
                    if (root.recording)
                        recorderFocus.forceActiveFocus()
                }
            }
        }

        // Hidden focus catcher for interactive key recording
        Item {
            id: recorderFocus
            width: 0; height: 0
            focus: root.recording
            Keys.onPressed: function(event) {
                if (!root.recording) return
                if (event.key === Qt.Key_Escape) {
                    root.recording = false
                    event.accepted = true
                    return
                }
                var keyName = root.keyToName(event.key)
                if (!keyName) return
                var parts = []
                if (event.modifiers & Qt.MetaModifier) parts.push("SUPER")
                if (event.modifiers & Qt.ShiftModifier) parts.push("SHIFT")
                if (event.modifiers & Qt.ControlModifier) parts.push("CTRL")
                if (event.modifiers & Qt.AltModifier) parts.push("ALT")
                parts.push(keyName)
                root.comboValue = parts.join(" + ")
                root.recording = false
                event.accepted = true
            }
        }

        // Conflict notice
        Label {
            Layout.fillWidth: true
            readonly property string conflict: root.comboValue.trim().length > 0
                ? backend.findConflict(root.comboValue) : ""
            visible: conflict.length > 0
            text: qsTr("Already bound to %1 — saving will replace it.").arg(conflict)
            color: "#ffd666"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        TextField {
            id: labelField
            Layout.fillWidth: true
            text: root.descriptionValue
            placeholderText: qsTr("App or label (type to search installed apps)")
            Material.roundedScale: theme.radius
            onTextChanged: root.descriptionValue = text
        }

        // Quick-pick installed apps matching what the user typed
        Flow {
            Layout.fillWidth: true
            spacing: 6
            property var matches: {
                var q = root.descriptionValue.trim().toLowerCase()
                if (q.length < 2) return []
                var list = backend.allApps
                var out = []
                for (var i = 0; i < list.length && out.length < 5; ++i) {
                    var app = list[i]
                    if (app.name.toLowerCase().indexOf(q) !== -1 || app.command.toLowerCase().indexOf(q) !== -1) {
                        if (app.name.toLowerCase() !== q || root.commandValue.trim().length === 0)
                            out.push(app)
                    }
                }
                return out
            }
            visible: matches.length > 0

            Repeater {
                model: parent.matches
                delegate: Rectangle {
                    required property var modelData
                    color: pickHover.hovered ? "#2a2a34" : "#202028"
                    border.color: "#343440"
                    border.width: 1
                    radius: Math.min(4, theme.radius)
                    implicitWidth: pickLbl.implicitWidth + 16
                    implicitHeight: 26

                    Label {
                        id: pickLbl
                        anchors.centerIn: parent
                        text: modelData.name + "  (" + modelData.command + ")"
                        font.pixelSize: 11
                        color: "#d4d4dc"
                    }
                    HoverHandler { id: pickHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        onTapped: {
                            root.descriptionValue = modelData.name
                            root.commandValue = modelData.command
                            if (root.comboValue.trim().length === 0)
                                root.comboValue = modelData.suggestedCombo
                            root.launchMode = true
                        }
                    }
                }
            }
        }

        TextField {
            Layout.fillWidth: true
            text: root.commandValue
            placeholderText: qsTr("Command or binary (e.g. zen-browser)")
            font.family: "monospace"
            Material.roundedScale: theme.radius
            onTextChanged: root.commandValue = text
        }

        CheckBox {
            text: qsTr("Launch as desktop app (uwsm-app)")
            checked: root.launchMode
            Material.roundedScale: theme.radius
            onToggled: root.launchMode = checked
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Cancel")
                flat: true
                Material.roundedScale: theme.radius
                onClicked: root.close()
            }
            Button {
                text: qsTr("Save")
                highlighted: true
                enabled: root.comboValue.trim().length > 0 && root.commandValue.trim().length > 0
                Material.roundedScale: theme.radius
                onClicked: {
                    backend.bindApp(root.comboValue, root.descriptionValue, root.commandValue, root.launchMode)
                    root.close()
                }
            }
        }
    }
}
