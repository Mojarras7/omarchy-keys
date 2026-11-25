import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1040
    height: 700
    minimumWidth: 720
    minimumHeight: 500
    visible: true
    title: qsTr("Keys")
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property int currentTab: 0 // 0 = Recommend, 1 = All Bindings, 2 = Apps
    property bool helpVisible: false
    property string searchQuery: ""
    property string appSearchQuery: ""

    function filteredBindings() {
        var list = backend.bindings
        var q = searchQuery.trim().toLowerCase()
        if (!q) return list
        var out = []
        for (var i = 0; i < list.length; ++i) {
            var item = list[i]
            if (item.combo.toLowerCase().indexOf(q) !== -1 ||
                item.description.toLowerCase().indexOf(q) !== -1) {
                out.push(item)
            }
        }
        return out
    }

    function filteredApps() {
        var list = backend.allApps
        var q = appSearchQuery.trim().toLowerCase()
        if (!q) return list
        var out = []
        for (var i = 0; i < list.length; ++i) {
            var item = list[i]
            if (item.name.toLowerCase().indexOf(q) !== -1 ||
                item.command.toLowerCase().indexOf(q) !== -1 ||
                item.id.toLowerCase().indexOf(q) !== -1) {
                out.push(item)
            }
        }
        return out
    }

    function openBindDialog(combo, description, command, isLaunch) {
        keyDialog.openFor(combo, description, command, isLaunch)
    }

    readonly property bool typingInSearch: searchField.activeFocus || appSearchField.activeFocus

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !keyDialog.visible && !win.typingInSearch; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !keyDialog.visible && !win.typingInSearch; onActivated: Qt.quit() }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; onActivated: {
        if (win.helpVisible) win.helpVisible = false
        else if (keyDialog.visible) keyDialog.close()
        else if (searchField.activeFocus) searchField.focus = false
        else if (appSearchField.activeFocus) appSearchField.focus = false
    }}
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: !keyDialog.visible && !win.typingInSearch; onActivated: win.currentTab = 0 }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: !keyDialog.visible && !win.typingInSearch; onActivated: win.currentTab = 1 }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: !keyDialog.visible && !win.typingInSearch; onActivated: win.currentTab = 2 }
    Shortcut { sequence: "/"; context: Qt.ApplicationShortcut; enabled: !keyDialog.visible; onActivated: {
        if (win.currentTab === 2) {
            appSearchField.forceActiveFocus()
        } else {
            win.currentTab = 1
            searchField.forceActiveFocus()
        }
    }}
    Shortcut { sequence: "N"; context: Qt.ApplicationShortcut; enabled: !keyDialog.visible && !win.typingInSearch; onActivated: win.openBindDialog("", "", "", true) }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !keyDialog.visible && !win.typingInSearch; onActivated: backend.refresh() }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: !keyDialog.visible && backend.canUndo; onActivated: backend.undo() }
    Shortcut { sequences: [StandardKey.Redo]; context: Qt.ApplicationShortcut; enabled: !keyDialog.visible && backend.canRedo; onActivated: backend.redo() }

    // Right-to-left languages mirror the window; Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            // Header bar
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    spacing: 2
                    Label {
                        text: qsTr("Keys")
                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                        color: "#f4f4f6"
                    }
                    Label {
                        property string _lang: translator.language
                        text: qsTr("%n bindings", "", backend.totalBindings) + " · " +
                              qsTr("%n active apps", "", backend.trackedAppsCount)
                        font.pixelSize: 12
                        color: "#8e8e98"
                    }
                }

                Item { Layout.fillWidth: true }

                // Tab switcher
                Rectangle {
                    color: "#18181c"
                    border.color: "#282830"
                    border.width: 1
                    radius: theme.radius
                    implicitHeight: 36
                    implicitWidth: tabRow.implicitWidth + 8

                    RowLayout {
                        id: tabRow
                        anchors.centerIn: parent
                        spacing: 4

                        Repeater {
                            model: [
                                { label: qsTr("Recommend"), badge: backend.bindRecommendations.length + backend.unbindRecommendations.length },
                                { label: qsTr("All Bindings"), badge: backend.totalBindings },
                                { label: qsTr("Apps"), badge: backend.allApps.length }
                            ]
                            delegate: Rectangle {
                                required property int index
                                required property var modelData
                                readonly property bool active: win.currentTab === index
                                color: active ? theme.accent : (tabHover.hovered ? "#222228" : "transparent")
                                radius: Math.min(theme.radius, 6)
                                implicitHeight: 28
                                implicitWidth: tabBtnRow.implicitWidth + 20

                                RowLayout {
                                    id: tabBtnRow
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Label {
                                        text: modelData.label
                                        font.pixelSize: 13
                                        font.weight: active ? Font.DemiBold : Font.Normal
                                        color: active ? theme.accentForeground : "#d4d4dc"
                                    }
                                    Label {
                                        property string _lang: translator.language
                                        text: Number(modelData.badge).toLocaleString(Qt.locale(), 'f', 0)
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                        color: active ? theme.accentForeground : "#8e8e98"
                                        opacity: active ? 0.85 : 1.0
                                    }
                                }
                                HoverHandler { id: tabHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: win.currentTab = index }
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Action buttons
                Rectangle {
                    color: theme.accent
                    radius: theme.radius
                    implicitHeight: 34
                    implicitWidth: newBtnLbl.implicitWidth + 24
                    Label {
                        id: newBtnLbl
                        anchors.centerIn: parent
                        text: qsTr("New")
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: theme.accentForeground
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: win.openBindDialog("", "", "", true) }
                }

                Button {
                    text: qsTr("Undo")
                    flat: true
                    enabled: backend.canUndo
                    Material.roundedScale: theme.radius
                    onClicked: backend.undo()
                }

                Button {
                    text: qsTr("Refresh")
                    flat: true
                    enabled: !backend.busy
                    Material.roundedScale: theme.radius
                    onClicked: backend.refresh()
                }

                Button {
                    text: "?"
                    flat: true
                    implicitWidth: 36
                    Material.roundedScale: theme.radius
                    onClicked: win.helpVisible = !win.helpVisible
                }
            }

            // Status / error banner
            Rectangle {
                Layout.fillWidth: true
                visible: backend.errorMessage.length > 0 || backend.statusMessage.length > 0
                color: backend.errorMessage.length > 0 ? "#38181b" : "#1a231c"
                border.color: backend.errorMessage.length > 0 ? "#7a2c32" : "#2f4f35"
                border.width: 1
                radius: theme.radius
                implicitHeight: bannerRow.implicitHeight + 16

                RowLayout {
                    id: bannerRow
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 10

                    Label {
                        Layout.fillWidth: true
                        text: backend.errorMessage.length > 0 ? backend.errorMessage : backend.statusMessage
                        color: backend.errorMessage.length > 0 ? "#ff9ea4" : "#b5e8be"
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        text: qsTr("Dismiss")
                        flat: true
                        Material.roundedScale: theme.radius
                        onClicked: backend.clearMessages()
                    }
                }
            }

            // TAB 0: Recommendations (Bind + Unbind)
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 16
                visible: win.currentTab === 0

                // Left column: Recommended to bind
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#151519"
                    border.color: "#25252d"
                    border.width: 1
                    radius: theme.radius

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                spacing: 2
                                Label {
                                    text: qsTr("Recommended to bind")
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                    color: "#f4f4f6"
                                }
                                Label {
                                    text: qsTr("Most-used apps without a shortcut")
                                    font.pixelSize: 12
                                    color: "#8e8e98"
                                }
                            }
                            Item { Layout.fillWidth: true }
                            Rectangle {
                                color: theme.accent
                                radius: Math.min(4, theme.radius)
                                implicitWidth: bindCountLbl.implicitWidth + 14
                                implicitHeight: 22
                                Label {
                                    id: bindCountLbl
                                    anchors.centerIn: parent
                                    property string _lang: translator.language
                                    text: Number(backend.bindRecommendations.length).toLocaleString(Qt.locale(), 'f', 0)
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: theme.accentForeground
                                }
                            }
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 8
                            model: backend.bindRecommendations

                            delegate: Item {
                                required property var modelData
                                width: ListView.view.width
                                height: bindCard.implicitHeight

                                Rectangle {
                                    id: bindCard
                                    anchors.fill: parent
                                    color: "#1c1c22"
                                    border.color: "#2a2a33"
                                    border.width: 1
                                    radius: theme.radius
                                    implicitHeight: bindCardLayout.implicitHeight + 20

                                    RowLayout {
                                        id: bindCardLayout
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 10

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4

                                            RowLayout {
                                                spacing: 8
                                                Label {
                                                    text: modelData.name
                                                    font.pixelSize: 14
                                                    font.weight: Font.Medium
                                                    color: "#f4f4f6"
                                                    elide: Text.ElideRight
                                                    Layout.maximumWidth: 200
                                                }
                                                Rectangle {
                                                    visible: modelData.isRunning
                                                    color: "#1f3224"
                                                    border.color: "#33593d"
                                                    border.width: 1
                                                    radius: Math.min(4, theme.radius)
                                                    implicitWidth: runLbl.implicitWidth + 10
                                                    implicitHeight: 18
                                                    Label {
                                                        id: runLbl
                                                        anchors.centerIn: parent
                                                        text: qsTr("Open")
                                                        font.pixelSize: 10
                                                        color: "#8ce09e"
                                                    }
                                                }
                                            }

                                            Label {
                                                text: modelData.reason
                                                font.pixelSize: 12
                                                color: "#8e8e98"
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }

                                            Rectangle {
                                                color: "#121216"
                                                border.color: "#2e2e38"
                                                border.width: 1
                                                radius: Math.min(4, theme.radius)
                                                implicitWidth: sugLbl.implicitWidth + 12
                                                implicitHeight: 22
                                                Label {
                                                    id: sugLbl
                                                    anchors.centerIn: parent
                                                    text: modelData.suggestedCombo
                                                    font.pixelSize: 11
                                                    font.family: "monospace"
                                                    color: theme.accent
                                                }
                                            }
                                        }

                                        RowLayout {
                                            spacing: 6
                                            Rectangle {
                                                color: editHover.hovered ? "#2a2a34" : "#22222a"
                                                border.color: "#343440"
                                                border.width: 1
                                                radius: theme.radius
                                                implicitHeight: 30
                                                implicitWidth: editLbl.implicitWidth + 18
                                                Label {
                                                    id: editLbl
                                                    anchors.centerIn: parent
                                                    text: qsTr("Edit")
                                                    font.pixelSize: 12
                                                    color: "#d4d4dc"
                                                }
                                                HoverHandler { id: editHover; cursorShape: Qt.PointingHandCursor }
                                                TapHandler { onTapped: win.openBindDialog(modelData.suggestedCombo, modelData.name, modelData.command, true) }
                                            }
                                            Rectangle {
                                                color: theme.accent
                                                radius: theme.radius
                                                implicitHeight: 30
                                                implicitWidth: bindBtnLbl.implicitWidth + 20
                                                Label {
                                                    id: bindBtnLbl
                                                    anchors.centerIn: parent
                                                    text: qsTr("Bind")
                                                    font.pixelSize: 12
                                                    font.weight: Font.DemiBold
                                                    color: theme.accentForeground
                                                }
                                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                                TapHandler { onTapped: backend.bindApp(modelData.suggestedCombo, modelData.name, modelData.command, true) }
                                            }
                                        }
                                    }
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: parent.count === 0
                                text: qsTr("All your frequently used apps already have shortcuts.")
                                color: "#767680"
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                width: parent.width - 32
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }

                // Right column: Recommended to unbind
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#151519"
                    border.color: "#25252d"
                    border.width: 1
                    radius: theme.radius

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                spacing: 2
                                Label {
                                    text: qsTr("Recommended to unbind")
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                    color: "#f4f4f6"
                                }
                                Label {
                                    text: qsTr("Unused or uninstalled app shortcuts")
                                    font.pixelSize: 12
                                    color: "#8e8e98"
                                }
                            }
                            Item { Layout.fillWidth: true }
                            Rectangle {
                                color: "#2a2a33"
                                radius: Math.min(4, theme.radius)
                                implicitWidth: unbindCountLbl.implicitWidth + 14
                                implicitHeight: 22
                                Label {
                                    id: unbindCountLbl
                                    anchors.centerIn: parent
                                    property string _lang: translator.language
                                    text: Number(backend.unbindRecommendations.length).toLocaleString(Qt.locale(), 'f', 0)
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: "#d4d4dc"
                                }
                            }
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 8
                            model: backend.unbindRecommendations

                            delegate: Item {
                                required property var modelData
                                width: ListView.view.width
                                height: unbindCard.implicitHeight

                                Rectangle {
                                    id: unbindCard
                                    anchors.fill: parent
                                    color: "#1c1c22"
                                    border.color: !modelData.isInstalled ? "#542a2e" : "#2a2a33"
                                    border.width: 1
                                    radius: theme.radius
                                    implicitHeight: unbindCardLayout.implicitHeight + 20

                                    RowLayout {
                                        id: unbindCardLayout
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 10

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4

                                            RowLayout {
                                                spacing: 8
                                                Label {
                                                    text: modelData.description
                                                    font.pixelSize: 14
                                                    font.weight: Font.Medium
                                                    color: "#f4f4f6"
                                                    elide: Text.ElideRight
                                                }
                                                Rectangle {
                                                    visible: !modelData.isInstalled
                                                    color: "#381c1f"
                                                    border.color: "#6e3238"
                                                    border.width: 1
                                                    radius: Math.min(4, theme.radius)
                                                    implicitWidth: notInstLbl.implicitWidth + 10
                                                    implicitHeight: 18
                                                    Label {
                                                        id: notInstLbl
                                                        anchors.centerIn: parent
                                                        text: qsTr("Not installed")
                                                        font.pixelSize: 10
                                                        color: "#ff9ea4"
                                                    }
                                                }
                                            }

                                            Label {
                                                text: modelData.reason
                                                font.pixelSize: 12
                                                color: "#8e8e98"
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }

                                            Rectangle {
                                                color: "#121216"
                                                border.color: "#2e2e38"
                                                border.width: 1
                                                radius: Math.min(4, theme.radius)
                                                implicitWidth: unbindKeyLbl.implicitWidth + 12
                                                implicitHeight: 22
                                                Label {
                                                    id: unbindKeyLbl
                                                    anchors.centerIn: parent
                                                    text: modelData.combo
                                                    font.pixelSize: 11
                                                    font.family: "monospace"
                                                    color: "#c8c8d2"
                                                }
                                            }
                                        }

                                        Rectangle {
                                            color: unbindHover.hovered ? "#3a2024" : "#22222a"
                                            border.color: unbindHover.hovered ? "#7a363c" : "#343440"
                                            border.width: 1
                                            radius: theme.radius
                                            implicitHeight: 30
                                            implicitWidth: unbindBtnLbl.implicitWidth + 18
                                            Label {
                                                id: unbindBtnLbl
                                                anchors.centerIn: parent
                                                text: qsTr("Unbind")
                                                font.pixelSize: 12
                                                color: unbindHover.hovered ? "#ffadb3" : "#d4d4dc"
                                            }
                                            HoverHandler { id: unbindHover; cursorShape: Qt.PointingHandCursor }
                                            TapHandler { onTapped: backend.unbindKey(modelData.combo) }
                                        }
                                    }
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: parent.count === 0
                                text: qsTr("No unused app bindings found.")
                                color: "#767680"
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                width: parent.width - 32
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            // TAB 1: All Bindings
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12
                visible: win.currentTab === 1

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Filter by key or action (/)")
                        Material.roundedScale: theme.radius
                        onTextChanged: win.searchQuery = text
                    }
                }

                // Unbound overrides strip (allows restoring any unbound key)
                Rectangle {
                    Layout.fillWidth: true
                    visible: backend.unboundOverrides.length > 0
                    color: "#18181d"
                    border.color: "#2b2b34"
                    border.width: 1
                    radius: theme.radius
                    implicitHeight: unboundStrip.implicitHeight + 16

                    RowLayout {
                        id: unboundStrip
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Label {
                            text: qsTr("Unbound:")
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            color: "#8e8e98"
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: backend.unboundOverrides
                                delegate: Rectangle {
                                    required property var modelData
                                    color: "#22222a"
                                    border.color: "#363642"
                                    border.width: 1
                                    radius: Math.min(4, theme.radius)
                                    implicitWidth: chipRow.implicitWidth + 14
                                    implicitHeight: 26

                                    RowLayout {
                                        id: chipRow
                                        anchors.centerIn: parent
                                        spacing: 6
                                        Label {
                                            text: modelData.combo
                                            font.pixelSize: 11
                                            font.family: "monospace"
                                            color: "#c8c8d2"
                                        }
                                        Label {
                                            text: qsTr("Restore")
                                            font.pixelSize: 11
                                            font.weight: Font.Medium
                                            color: theme.accent
                                        }
                                    }
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: backend.restoreKey(modelData.combo) }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#151519"
                    border.color: "#25252d"
                    border.width: 1
                    radius: theme.radius

                    ListView {
                        anchors.fill: parent
                        anchors.margins: 12
                        clip: true
                        spacing: 6
                        model: win.filteredBindings()

                        delegate: Item {
                            required property var modelData
                            width: ListView.view.width
                            height: 48

                            Rectangle {
                                anchors.fill: parent
                                color: "#1c1c22"
                                border.color: modelData.isCustom ? theme.accent : "#26262f"
                                border.width: 1
                                radius: theme.radius

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 8
                                    spacing: 12

                                    Rectangle {
                                        color: "#121216"
                                        border.color: "#2e2e38"
                                        border.width: 1
                                        radius: Math.min(4, theme.radius)
                                        implicitWidth: 210
                                        implicitHeight: 26
                                        Label {
                                            anchors.centerIn: parent
                                            width: parent.width - 12
                                            text: modelData.combo
                                            font.pixelSize: 12
                                            font.family: "monospace"
                                            color: theme.accent
                                            elide: Text.ElideRight
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.description
                                        font.pixelSize: 14
                                        color: "#f0f0f4"
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        visible: modelData.isCustom
                                        text: qsTr("Custom")
                                        font.pixelSize: 11
                                        color: theme.accent
                                    }

                                    Button {
                                        text: qsTr("Rebind")
                                        flat: true
                                        Material.roundedScale: theme.radius
                                        onClicked: win.openBindDialog(modelData.combo, modelData.description, "", true)
                                    }

                                    Button {
                                        text: modelData.isCustom ? qsTr("Remove") : qsTr("Unbind")
                                        flat: true
                                        Material.roundedScale: theme.radius
                                        onClicked: backend.unbindKey(modelData.combo)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // TAB 2: Searchable Installed Apps (most used first)
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12
                visible: win.currentTab === 2

                TextField {
                    id: appSearchField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search installed apps to bind (/)")
                    Material.roundedScale: theme.radius
                    onTextChanged: win.appSearchQuery = text
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#151519"
                    border.color: "#25252d"
                    border.width: 1
                    radius: theme.radius

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        Label {
                            text: qsTr("Installed applications (most used first)")
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            color: "#f4f4f6"
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 8
                            model: win.filteredApps()

                            delegate: Item {
                                required property var modelData
                                width: ListView.view.width
                                height: modelData.launches > 0 ? 60 : 50

                                Rectangle {
                                    anchors.fill: parent
                                    color: "#1c1c22"
                                    border.color: "#2a2a33"
                                    border.width: 1
                                    radius: theme.radius

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 14
                                        anchors.rightMargin: 10
                                        spacing: 14

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 5

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 8
                                                Label {
                                                    text: modelData.name
                                                    font.pixelSize: 14
                                                    font.weight: Font.Medium
                                                    color: "#f4f4f6"
                                                    elide: Text.ElideRight
                                                }
                                                Label {
                                                    property string _lang: translator.language
                                                    text: modelData.launches > 0
                                                        ? qsTr("%n launches", "", modelData.launches)
                                                        : (modelData.isRunning ? qsTr("Open now") : modelData.command)
                                                    font.pixelSize: 12
                                                    color: "#8e8e98"
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }
                                            }

                                            Rectangle {
                                                visible: modelData.launches > 0
                                                Layout.fillWidth: true
                                                height: 6
                                                color: "#121216"
                                                radius: Math.min(3, theme.radius)

                                                Rectangle {
                                                    width: Math.max(8, parent.width * modelData.ratio)
                                                    height: parent.height
                                                    color: theme.accent
                                                    radius: parent.radius
                                                }
                                            }
                                        }

                                        Rectangle {
                                            visible: modelData.isBound
                                            color: "#121216"
                                            border.color: "#2e2e38"
                                            border.width: 1
                                            radius: Math.min(4, theme.radius)
                                            implicitWidth: boundLbl.implicitWidth + 16
                                            implicitHeight: 28
                                            Label {
                                                id: boundLbl
                                                anchors.centerIn: parent
                                                text: modelData.boundCombo
                                                font.pixelSize: 11
                                                font.family: "monospace"
                                                color: "#c8c8d2"
                                            }
                                        }

                                        RowLayout {
                                            spacing: 6
                                            Rectangle {
                                                visible: modelData.isBound
                                                color: appUnbindHover.hovered ? "#3a2024" : "#22222a"
                                                border.color: appUnbindHover.hovered ? "#7a363c" : "#343440"
                                                border.width: 1
                                                radius: theme.radius
                                                implicitHeight: 30
                                                implicitWidth: appUnbindLbl.implicitWidth + 18
                                                Label {
                                                    id: appUnbindLbl
                                                    anchors.centerIn: parent
                                                    text: qsTr("Unbind")
                                                    font.pixelSize: 12
                                                    color: appUnbindHover.hovered ? "#ffadb3" : "#d4d4dc"
                                                }
                                                HoverHandler { id: appUnbindHover; cursorShape: Qt.PointingHandCursor }
                                                TapHandler { onTapped: backend.unbindKey(modelData.boundCombo) }
                                            }
                                            Rectangle {
                                                color: modelData.isBound ? (rebindAppHover.hovered ? "#2a2a34" : "#22222a") : theme.accent
                                                border.color: modelData.isBound ? "#343440" : "transparent"
                                                border.width: modelData.isBound ? 1 : 0
                                                radius: theme.radius
                                                implicitHeight: 30
                                                implicitWidth: appBindLbl.implicitWidth + 20
                                                Label {
                                                    id: appBindLbl
                                                    anchors.centerIn: parent
                                                    text: modelData.isBound ? qsTr("Rebind") : qsTr("Bind")
                                                    font.pixelSize: 12
                                                    font.weight: modelData.isBound ? Font.Normal : Font.DemiBold
                                                    color: modelData.isBound ? "#d4d4dc" : theme.accentForeground
                                                }
                                                HoverHandler { id: rebindAppHover; cursorShape: Qt.PointingHandCursor }
                                                TapHandler { onTapped: win.openBindDialog(modelData.suggestedCombo, modelData.name, modelData.command, true) }
                                            }
                                        }
                                    }
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: parent.count === 0
                                text: qsTr("No matching apps found.")
                                color: "#767680"
                                font.pixelSize: 13
                            }
                        }
                    }
                }
            }
        }

        KeyDialog {
            id: keyDialog
        }

        HelpOverlay {
            id: helpOverlay
            visible: win.helpVisible
            onCloseRequested: win.helpVisible = false
        }
    }
}
