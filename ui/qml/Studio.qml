import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Item {
    id: root
    anchors.fill: parent

    property color canvasColor: "#101619"
    property color panelColor: "#182126"
    property color panelEdge: "#2A383E"
    property color accent: "#B8E85A"
    property string renameSceneId: ""
    property string renameSourceId: ""

    Rectangle { anchors.fill: parent; color: root.canvasColor }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text { text: "Studio Workspace"; color: "#F4F7F8"; font.pixelSize: 24; font.weight: Font.DemiBold }
                Text { text: "Build the program before you go live"; color: "#89979E"; font.pixelSize: 12 }
            }

            Rectangle {
                Layout.preferredWidth: 210
                Layout.preferredHeight: 42
                radius: 6
                color: "#1D292E"
                border.color: root.panelEdge
                Row {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8
                    Rectangle { width: 8; height: 8; radius: 4; anchors.verticalCenter: parent.verticalCenter; color: root.accent }
                    Text { anchors.verticalCenter: parent.verticalCenter; text: studioController.profileName; color: "#DDE6E8"; font.pixelSize: 13; elide: Text.ElideRight; width: 150 }
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: profileDialog.open() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            ColumnLayout {
                Layout.preferredWidth: 248
                Layout.minimumWidth: 210
                Layout.fillHeight: true
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 220
                    radius: 7
                    color: root.panelColor
                    border.color: root.panelEdge

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10
                        PanelHeader { title: "Scenes"; subtitle: "Your program layouts"; actionText: "+"; onActionClicked: { sceneNameField.text = ""; sceneDialog.open() } }
                        ListView {
                            id: scenesView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 4
                            model: studioController.scenesModel
                            delegate: Rectangle {
                                required property string sceneId
                                required property string name
                                required property bool active
                                width: scenesView.width
                                height: 44
                                radius: 5
                                color: active ? "#31413B" : (sceneMouse.containsMouse ? "#213036" : "transparent")
                                border.color: active ? "#627A57" : "transparent"
                                Text { anchors.left: parent.left; anchors.leftMargin: 11; anchors.verticalCenter: parent.verticalCenter; width: 125; text: name; elide: Text.ElideRight; color: active ? "#F1F6EA" : "#C2CED1"; font.pixelSize: 13; font.weight: active ? Font.DemiBold : Font.Normal }
                                Row { anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; spacing: 1
                                    ToolButton { text: "^"; visible: sceneMouse.containsMouse; onClicked: studioController.moveScene(sceneId, -1); ToolTip.visible: hovered; ToolTip.text: "Move scene up" }
                                    ToolButton { text: "v"; visible: sceneMouse.containsMouse; onClicked: studioController.moveScene(sceneId, 1); ToolTip.visible: hovered; ToolTip.text: "Move scene down" }
                                    ToolButton { text: "x"; visible: sceneMouse.containsMouse; onClicked: studioController.deleteScene(sceneId); ToolTip.visible: hovered; ToolTip.text: "Delete scene" }
                                }
                                MouseArea { id: sceneMouse; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.LeftButton; onClicked: studioController.selectScene(sceneId); onDoubleClicked: { root.renameSceneId = sceneId; renameSceneField.text = name; renameSceneDialog.open() } }
                            }
                        }
                        Text { Layout.fillWidth: true; visible: scenesView.count < 2; text: "Add scenes for each part of your stream."; color: "#7F8D93"; font.pixelSize: 11; wrapMode: Text.WordWrap }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 300
                    radius: 7
                    color: root.panelColor
                    border.color: root.panelEdge
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10
                        PanelHeader { title: "Sources"; subtitle: "Layers in " + studioController.activeSceneName; actionText: "+"; onActionClicked: addSourceMenu.open() }
                        ListView {
                            id: sourcesView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 4
                            model: studioController.sceneItemsModel
                            delegate: Rectangle {
                                required property string itemId
                                required property string sourceId
                                required property string name
                                required property string type
                                required property bool itemVisible
                                required property bool itemLocked
                                required property bool selected
                                width: sourcesView.width
                                height: 47
                                radius: 5
                                color: selected ? "#27393A" : (sourceMouse.containsMouse ? "#203038" : "transparent")
                                border.color: selected ? "#5E8A86" : "transparent"
                                Column { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; width: 120; spacing: 2
                                    Text { width: parent.width; text: name; elide: Text.ElideRight; color: itemVisible ? "#E2E9EA" : "#77868C"; font.pixelSize: 12; font.weight: Font.DemiBold }
                                    Text { width: parent.width; text: type + " - placeholder"; elide: Text.ElideRight; color: "#7D8A90"; font.pixelSize: 10 }
                                }
                                Row { anchors.right: parent.right; anchors.rightMargin: 4; anchors.verticalCenter: parent.verticalCenter; spacing: 0
                                    ToolButton { text: itemVisible ? "O" : "-"; onClicked: studioController.setItemVisible(itemId, !itemVisible); ToolTip.visible: hovered; ToolTip.text: itemVisible ? "Hide source" : "Show source" }
                                    ToolButton { text: itemLocked ? "L" : "U"; onClicked: studioController.setItemLocked(itemId, !itemLocked); ToolTip.visible: hovered; ToolTip.text: itemLocked ? "Unlock source" : "Lock source" }
                                    ToolButton { text: "^"; visible: sourceMouse.containsMouse; onClicked: studioController.moveSceneItem(itemId, 1); ToolTip.visible: hovered; ToolTip.text: "Move layer forward" }
                                    ToolButton { text: "v"; visible: sourceMouse.containsMouse; onClicked: studioController.moveSceneItem(itemId, -1); ToolTip.visible: hovered; ToolTip.text: "Move layer backward" }
                                    ToolButton { text: "x"; visible: sourceMouse.containsMouse; onClicked: studioController.removeSceneItem(itemId); ToolTip.visible: hovered; ToolTip.text: "Remove source from this scene" }
                                }
                                MouseArea { id: sourceMouse; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.LeftButton; onClicked: studioController.selectItem(itemId); onDoubleClicked: { root.renameSourceId = sourceId; renameSourceField.text = name; renameSourceDialog.open() } }
                            }
                        }
                        Text { visible: sourcesView.count === 0; Layout.fillWidth: true; text: "Add a placeholder source to shape this scene."; color: "#7F8D93"; font.pixelSize: 11; wrapMode: Text.WordWrap }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 480
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 7
                    color: "#121A1D"
                    border.color: "#334148"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10
                        RowLayout { Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: studioController.activeSceneName; color: "#EAF0F1"; font.pixelSize: 14; font.weight: Font.DemiBold }
                            Text { text: "PROGRAM PREVIEW"; color: "#8A9A9F"; font.pixelSize: 10; font.weight: Font.DemiBold }
                        }
                        Item {
                            id: previewFrame
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                            implicitWidth: 720
                            implicitHeight: 405
                            Rectangle {
                                id: previewCanvas
                                anchors.centerIn: parent
                                width: Math.min(parent.width, parent.height * 16 / 9)
                                height: width * 9 / 16
                                color: "#0B1012"
                                border.color: "#425158"
                                border.width: 1
                                Rectangle { anchors.fill: parent; anchors.margins: 1; color: "#142025" }
                                Repeater {
                                    model: studioController.sceneItemsModel
                                    delegate: Rectangle {
                                        required property string itemId
                                        required property string name
                                        required property string type
                                        required property bool itemVisible
                                        required property bool itemLocked
                                        required property bool selected
                                        x: previewCanvas.width * (0.08 + (index % 3) * 0.12)
                                        y: previewCanvas.height * (0.12 + (index % 3) * 0.10)
                                        width: previewCanvas.width * (index === 0 ? 0.72 : 0.32)
                                        height: previewCanvas.height * (index === 0 ? 0.65 : 0.24)
                                        visible: itemVisible
                                        radius: 3
                                        color: type === "Image" ? "#8C4F60" : (type === "Text" ? "#60518D" : "#315E65")
                                        border.color: selected ? root.accent : (itemLocked ? "#A3ABB0" : "#7A8A90")
                                        border.width: selected ? 2 : 1
                                        opacity: 0.88
                                        Text { anchors.centerIn: parent; width: parent.width - 12; horizontalAlignment: Text.AlignHCenter; text: name + "\n" + type; color: "#F2F6F6"; font.pixelSize: 12; font.weight: Font.DemiBold; wrapMode: Text.WordWrap }
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: studioController.selectItem(itemId) }
                                    }
                                }
                                Column { anchors.centerIn: parent; visible: sourcesView.count === 0; spacing: 8
                                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Your scene starts here"; color: "#DFE7E7"; font.pixelSize: 18; font.weight: Font.DemiBold }
                                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Add sources to compose a future program feed."; color: "#8D9A9F"; font.pixelSize: 12 }
                                }
                                Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.bottom: parent.bottom; anchors.bottomMargin: 8; text: "1920 x 1080  |  Placeholder composition"; color: "#8A979B"; font.pixelSize: 10 }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 54
                    radius: 7
                    color: root.panelColor
                    border.color: root.panelEdge
                    RowLayout { anchors.fill: parent; anchors.margins: 12; spacing: 10
                        Rectangle { width: 8; height: 8; radius: 4; color: root.accent }
                        Text { Layout.fillWidth: true; text: studioController.statusMessage; color: "#AAB8BA"; font.pixelSize: 11; elide: Text.ElideRight }
                        Text { text: "Saved"; color: root.accent; font.pixelSize: 11; font.weight: Font.DemiBold }
                    }
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 288
                Layout.minimumWidth: 248
                Layout.fillHeight: true
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 250
                    radius: 7
                    color: root.panelColor
                    border.color: root.panelEdge
                    ColumnLayout { anchors.fill: parent; anchors.margins: 14; spacing: 11
                        PanelHeader { title: "Audio Mixer"; subtitle: "Inputs are inactive until audio setup" }
                        ListView { id: mixerView; Layout.fillWidth: true; Layout.fillHeight: true; spacing: 10; clip: true; model: studioController.mixerModel
                            delegate: ColumnLayout {
                                required property string channelId
                                required property string name
                                required property double volume
                                required property bool muted
                                width: mixerView.width; spacing: 5
                                RowLayout { Layout.fillWidth: true
                                    Text { Layout.fillWidth: true; text: name; color: muted ? "#7B898D" : "#D9E2E3"; font.pixelSize: 12; font.weight: Font.DemiBold }
                                    ToolButton { text: muted ? "OFF" : "ON"; onClicked: studioController.setMixerMuted(channelId, !muted); ToolTip.visible: hovered; ToolTip.text: muted ? "Unmute channel" : "Mute channel" }
                                }
                                RowLayout { Layout.fillWidth: true; spacing: 7
                                    Rectangle { Layout.preferredWidth: 34; Layout.preferredHeight: 5; radius: 2; color: "#354349"; Rectangle { width: parent.width * 0.35; height: parent.height; radius: 2; color: muted ? "#59666B" : "#7A9581" } }
                                    Slider { Layout.fillWidth: true; from: 0; to: 1; value: volume; onMoved: studioController.setMixerVolume(channelId, value) }
                                    Text { text: Math.round(volume * 100) + "%"; color: "#849297"; font.pixelSize: 10; width: 30 }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 142
                    radius: 7
                    color: root.panelColor
                    border.color: root.panelEdge
                    ColumnLayout { anchors.fill: parent; anchors.margins: 14; spacing: 8
                        PanelHeader { title: "Transition"; subtitle: "Applied when scenes change" }
                        RowLayout { Layout.fillWidth: true
                            ComboBox { Layout.fillWidth: true; model: ["Cut", "Fade"]; currentIndex: studioController.transitionType === "Cut" ? 0 : 1; onActivated: studioController.setTransitionType(currentText) }
                            Text { text: studioController.transitionType === "Cut" ? "Instant" : studioController.transitionDurationMs + " ms"; color: "#AAB6B8"; font.pixelSize: 11 }
                        }
                        Slider { Layout.fillWidth: true; enabled: studioController.transitionType === "Fade"; from: 0; to: 2000; stepSize: 50; value: studioController.transitionDurationMs; onMoved: studioController.setTransitionDurationMs(value) }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 190
                    radius: 7
                    color: "#1A282B"
                    border.color: "#395055"
                    ColumnLayout { anchors.fill: parent; anchors.margins: 14; spacing: 8
                        PanelHeader { title: "Controls"; subtitle: "Output is not connected yet" }
                        Button { Layout.fillWidth: true; text: "Start streaming"; enabled: false }
                        Button { Layout.fillWidth: true; text: "Start recording"; enabled: false }
                        Button { Layout.fillWidth: true; text: "Record + stream"; enabled: false }
                    }
                }
            }
        }
    }

    Menu {
        id: addSourceMenu
        title: "Add placeholder source"
        Repeater { model: ["Display Capture", "Window Capture", "Game Capture", "Webcam", "Image", "Text", "Microphone", "Desktop Audio", "Browser Source", "Media Source"]
            delegate: MenuItem { required property string modelData; text: modelData + (modelData === "Image" || modelData === "Text" ? "" : " (placeholder)"); onTriggered: studioController.addSource(modelData) }
        }
    }

    Dialog {
        id: sceneDialog; modal: true; title: "Create scene"; standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        onAccepted: studioController.addScene(sceneNameField.text)
        contentItem: TextField { id: sceneNameField; placeholderText: "Scene name"; selectByMouse: true; implicitWidth: 300 }
    }
    Dialog {
        id: renameSceneDialog; modal: true; title: "Rename scene"; standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        onAccepted: studioController.renameScene(root.renameSceneId, renameSceneField.text)
        contentItem: TextField { id: renameSceneField; placeholderText: "Scene name"; selectByMouse: true; implicitWidth: 300 }
    }
    Dialog {
        id: renameSourceDialog; modal: true; title: "Rename source"; standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        onAccepted: studioController.renameSource(root.renameSourceId, renameSourceField.text)
        contentItem: TextField { id: renameSourceField; placeholderText: "Source name"; selectByMouse: true; implicitWidth: 300 }
    }
    Dialog {
        id: profileDialog; modal: true; title: "Rename profile"; standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        onOpened: profileNameField.text = studioController.profileName
        onAccepted: studioController.setProfileName(profileNameField.text)
        contentItem: TextField { id: profileNameField; placeholderText: "Profile name"; selectByMouse: true; implicitWidth: 300 }
    }
}
