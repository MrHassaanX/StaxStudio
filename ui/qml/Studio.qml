import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Item {
    id: root
    anchors.fill: parent
    readonly property color accent: "#B7E65C"
    property string renameSceneId: ""
    property string renameSourceId: ""
    property string deleteSceneId: ""
    property string removeItemId: ""

    Rectangle { anchors.fill: parent; color: "#0F171A" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text { text: "Studio"; color: "#F5F8F8"; font.pixelSize: 22; font.weight: Font.DemiBold }
                Text { text: "Arrange your program, then connect media when you are ready."; color: "#829399"; font.pixelSize: 12 }
            }
            Button {
                Layout.preferredWidth: 206
                Layout.preferredHeight: 38
                background: Rectangle { radius: 5; color: parent.hovered ? "#203137" : "#1B292E"; border.color: "#30434A" }
                contentItem: Row { anchors.fill: parent; anchors.margins: 10; spacing: 8
                    Rectangle { width: 7; height: 7; radius: 4; anchors.verticalCenter: parent.verticalCenter; color: root.accent }
                    Text { anchors.verticalCenter: parent.verticalCenter; width: parent.width - 20; text: studioController.profileName; color: "#DCE7E8"; font.pixelSize: 12; elide: Text.ElideRight }
                }
                onClicked: profileDialog.open()
                ToolTip.visible: hovered
                ToolTip.text: "Rename studio profile"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            ColumnLayout {
                Layout.preferredWidth: 222
                Layout.minimumWidth: 200
                Layout.fillHeight: true
                spacing: 12
                StudioPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 220
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10
                        StudioSectionHeader {
                            title: "Scenes"; subtitle: "Program layouts"
                            IconButton { iconName: "add"; tooltip: "Add scene"; onClicked: { sceneNameField.text = ""; sceneDialog.open() } }
                        }
                        ListView {
                            id: scenesView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 3
                            model: studioController.scenesModel
                            delegate: SceneRow {
                                required property string sceneId
                                required property string name
                                required property bool active
                                width: scenesView.width
                                onSelected: studioController.selectScene(sceneId)
                                onRenameRequested: { root.renameSceneId = sceneId; renameSceneField.text = name; renameSceneDialog.open() }
                                onDeleteRequested: { root.deleteSceneId = sceneId; deleteSceneDialog.open() }
                                onMoveRequested: direction => studioController.moveScene(sceneId, direction)
                            }
                        }
                    }
                }
                StudioPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 282
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10
                        StudioSectionHeader {
                            title: "Sources"; subtitle: "Layers in " + studioController.activeSceneName
                            IconButton { iconName: "add"; tooltip: "Add source"; onClicked: addSourceMenu.open() }
                        }
                        ListView {
                            id: sourcesView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 4
                            model: studioController.sceneItemsModel
                            delegate: SourceRow {
                                required property string itemId
                                required property string sourceId
                                required property string name
                                required property string type
                                required property bool itemVisible
                                required property bool itemLocked
                                required property bool selected
                                width: sourcesView.width
                                onSelectedRequested: studioController.selectItem(itemId)
                                onRenameRequested: { root.renameSourceId = sourceId; renameSourceField.text = name; renameSourceDialog.open() }
                                onVisibleRequested: value => studioController.setItemVisible(itemId, value)
                                onLockedRequested: value => studioController.setItemLocked(itemId, value)
                                onMoveRequested: direction => studioController.moveSceneItem(itemId, direction)
                                onRemoveRequested: { root.removeItemId = itemId; removeSourceDialog.open() }
                            }
                        }
                        Text { Layout.fillWidth: true; visible: sourcesView.count === 0; text: "Add a source to start composing this scene."; color: "#829399"; font.pixelSize: 11; wrapMode: Text.WordWrap }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 470
                spacing: 12
                StudioPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentMargin: 14
                    color: "#131E22"
                    border.color: "#30434A"
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: studioController.activeSceneName; color: "#F3F7F7"; font.pixelSize: 14; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { text: "PROGRAM PREVIEW"; color: "#8FA4A8"; font.pixelSize: 10; font.weight: Font.DemiBold }
                        }
                        Item {
                            id: previewFrame
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 260
                            clip: true
                            Rectangle {
                                id: previewCanvas
                                anchors.centerIn: parent
                                width: Math.min(parent.width, parent.height * 16 / 9)
                                height: Math.round(width * 9 / 16)
                                color: "#0A0F11"
                                border.color: "#48616A"
                                border.width: 1
                                clip: true
                                Item {
                                    id: programLayer
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    clip: true
                                    Rectangle { anchors.fill: parent; color: "#152329" }
                                    Repeater {
                                        model: studioController.sceneItemsModel
                                        delegate: Rectangle {
                                            required property string itemId
                                            required property string name
                                            required property string type
                                            required property bool itemVisible
                                            required property bool itemLocked
                                            required property bool selected
                                            required property bool visual
                                            required property real programX
                                            required property real programY
                                            required property real programWidth
                                            required property real programHeight
                                            x: Math.max(0, programX / 1920 * programLayer.width)
                                            y: Math.max(0, programY / 1080 * programLayer.height)
                                            width: Math.min(programLayer.width - x, Math.max(0, programWidth / 1920 * programLayer.width))
                                            height: Math.min(programLayer.height - y, Math.max(0, programHeight / 1080 * programLayer.height))
                                            z: zOrder + 1
                                            visible: itemVisible && visual && width > 0 && height > 0
                                            radius: 2
                                            color: type === "Image" ? "#704251" : (type === "Text" ? "#4A4670" : "#284F58")
                                            border.color: selected ? root.accent : (itemLocked ? "#AAB5B7" : "#66858C")
                                            border.width: selected ? 2 : 1
                                            opacity: 0.92
                                            Text { anchors.centerIn: parent; width: Math.max(0, parent.width - 20); text: name; color: "#F2F7F7"; font.pixelSize: 13; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: studioController.selectItem(itemId) }
                                        }
                                    }
                                    Column {
                                        anchors.centerIn: parent
                                        visible: sourcesView.count === 0
                                        spacing: 7
                                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Start with a source"; color: "#E1EAEB"; font.pixelSize: 18; font.weight: Font.DemiBold }
                                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "The preview will show your future program composition."; color: "#91A2A6"; font.pixelSize: 12 }
                                    }
                                }
                                Text { anchors.left: parent.left; anchors.leftMargin: 9; anchors.bottom: parent.bottom; anchors.bottomMargin: 7; text: "1920 x 1080"; color: "#8BA0A5"; font.pixelSize: 10; z: 10 }
                            }
                        }
                    }
                }
                StudioPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    RowLayout { anchors.fill: parent; spacing: 8
                        Rectangle { width: 8; height: 8; radius: 4; color: root.accent }
                        Text { Layout.fillWidth: true; text: studioController.statusMessage; color: "#AEBEC1"; font.pixelSize: 11; elide: Text.ElideRight }
                        Text { text: "Saved locally"; color: root.accent; font.pixelSize: 11; font.weight: Font.DemiBold }
                    }
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 264
                Layout.minimumWidth: 240
                Layout.fillHeight: true
                spacing: 12
                StudioPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 216
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10
                        StudioSectionHeader { title: "Audio Mixer"; subtitle: "Inputs are inactive" }
                        ListView {
                            id: mixerView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 12
                            model: studioController.mixerModel
                            delegate: MixerChannel {
                                required property string channelId
                                required property string name
                                required property real volume
                                required property bool muted
                                width: mixerView.width
                                onVolumeChangedByUser: value => studioController.setMixerVolume(channelId, value)
                                onMuteRequested: value => studioController.setMixerMuted(channelId, value)
                            }
                        }
                    }
                }
                StudioPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 150
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8
                        StudioSectionHeader { title: "Transition"; subtitle: "Between scenes" }
                        ComboBox { Layout.fillWidth: true; Layout.preferredHeight: 34; model: ["Cut", "Fade"]; currentIndex: studioController.transitionType === "Cut" ? 0 : 1; onActivated: studioController.setTransitionType(currentText) }
                        RowLayout { Layout.fillWidth: true; visible: studioController.transitionType === "Fade"
                            Text { text: "Duration"; color: "#AAB9BC"; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Text { text: studioController.transitionDurationMs + " ms"; color: "#D5E0E1"; font.pixelSize: 11 }
                        }
                        Slider { Layout.fillWidth: true; visible: studioController.transitionType === "Fade"; from: 0; to: 2000; stepSize: 50; value: studioController.transitionDurationMs; onMoved: studioController.setTransitionDurationMs(value) }
                    }
                }
                StudioPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    color: "#172529"
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8
                        StudioSectionHeader { title: "Controls"; subtitle: "Output is not connected" }
                        StudioButton { Layout.fillWidth: true; text: "Start streaming"; enabled: false }
                        StudioButton { Layout.fillWidth: true; text: "Start recording"; enabled: false }
                        StudioButton { Layout.fillWidth: true; text: "Record + stream"; enabled: false }
                    }
                }
            }
        }
    }

    Menu {
        id: addSourceMenu
        title: "Add placeholder source"
        Repeater { model: ["Display Capture", "Window Capture", "Game Capture", "Webcam", "Image", "Text", "Microphone", "Desktop Audio", "Browser Source", "Media Source"]
            delegate: MenuItem { required property string modelData; text: modelData; onTriggered: studioController.addSource(modelData) }
        }
    }
    Dialog {
        id: sceneDialog; modal: true; title: "Create scene"; standardButtons: Dialog.Ok | Dialog.Cancel; anchors.centerIn: parent
        onAccepted: studioController.addScene(sceneNameField.text)
        contentItem: TextField { id: sceneNameField; placeholderText: "Scene name"; selectByMouse: true; implicitWidth: 300 }
    }
    Dialog {
        id: renameSceneDialog; modal: true; title: "Rename scene"; standardButtons: Dialog.Ok | Dialog.Cancel; anchors.centerIn: parent
        onAccepted: studioController.renameScene(root.renameSceneId, renameSceneField.text)
        contentItem: TextField { id: renameSceneField; placeholderText: "Scene name"; selectByMouse: true; implicitWidth: 300 }
    }
    Dialog {
        id: renameSourceDialog; modal: true; title: "Rename source"; standardButtons: Dialog.Ok | Dialog.Cancel; anchors.centerIn: parent
        onAccepted: studioController.renameSource(root.renameSourceId, renameSourceField.text)
        contentItem: TextField { id: renameSourceField; placeholderText: "Source name"; selectByMouse: true; implicitWidth: 300 }
    }
    Dialog {
        id: profileDialog; modal: true; title: "Rename profile"; standardButtons: Dialog.Ok | Dialog.Cancel; anchors.centerIn: parent
        onOpened: profileNameField.text = studioController.profileName
        onAccepted: studioController.setProfileName(profileNameField.text)
        contentItem: TextField { id: profileNameField; placeholderText: "Profile name"; selectByMouse: true; implicitWidth: 300 }
    }
    Dialog {
        id: deleteSceneDialog; modal: true; title: "Delete scene?"; standardButtons: Dialog.Ok | Dialog.Cancel; anchors.centerIn: parent
        contentItem: Text { text: "This scene and its layout will be removed."; color: "#DCE6E7"; width: 280; wrapMode: Text.WordWrap }
        onAccepted: studioController.deleteScene(root.deleteSceneId)
    }
    Dialog {
        id: removeSourceDialog; modal: true; title: "Remove source?"; standardButtons: Dialog.Ok | Dialog.Cancel; anchors.centerIn: parent
        contentItem: Text { text: "This removes the source from the current scene."; color: "#DCE6E7"; width: 280; wrapMode: Text.WordWrap }
        onAccepted: studioController.removeSceneItem(root.removeItemId)
    }
}
