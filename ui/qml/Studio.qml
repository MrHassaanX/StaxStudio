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
    property string removeSourceId: ""
    property string propertiesSourceId: ""
    property string propertiesSourceType: ""
    property string selectedSourceType: ""

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
            Rectangle {
                Layout.preferredWidth: 180
                Layout.preferredHeight: 38
                radius: 5
                color: "#1B292E"
                border.color: "#30434A"
                Row { anchors.fill: parent; anchors.margins: 10; spacing: 8
                    Rectangle { width: 7; height: 7; radius: 4; anchors.verticalCenter: parent.verticalCenter; color: root.accent }
                    Text { anchors.verticalCenter: parent.verticalCenter; text: "PROJECT"; color: "#7F959A"; font.pixelSize: 10; font.weight: Font.DemiBold }
                    Text { anchors.verticalCenter: parent.verticalCenter; width: parent.width - 70; text: studioController.profileName; color: "#DCE7E8"; font.pixelSize: 12; elide: Text.ElideRight }
                }
            }
            Rectangle {
                Layout.preferredWidth: 164
                Layout.preferredHeight: 38
                radius: 5
                color: "#172328"
                border.color: "#2F4148"
                Row { anchors.fill: parent; anchors.margins: 10; spacing: 7
                    Text { anchors.verticalCenter: parent.verticalCenter; text: "SMART MODE"; color: "#A9B9BD"; font.pixelSize: 10; font.weight: Font.DemiBold }
                    Text { anchors.verticalCenter: parent.verticalCenter; text: "Not configured"; color: "#74878D"; font.pixelSize: 10 }
                }
                ToolTip.visible: smartModeArea.containsMouse
                ToolTip.text: "Hardware recommendations will be available after device setup."
                MouseArea { id: smartModeArea; anchors.fill: parent; hoverEnabled: true }
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
                            objectName: "scenesView"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 3
                            model: studioController.scenesModel
                            currentIndex: studioController.activeSceneIndex
                            onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
                            delegate: SceneRow {
                                required property int index
                                canDelete: scenesView.count > 1
                                canMoveUp: index > 0
                                canMoveDown: index < scenesView.count - 1
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
                            IconButton { iconName: "add"; tooltip: "Add source"; onClicked: { root.selectedSourceType = ""; sourceNameField.text = ""; sourcePickerDialog.open() } }
                        }
                        ListView {
                            id: sourcesView
                            objectName: "sourcesView"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 4
                            model: studioController.sceneItemsModel
                            delegate: SourceRow {
                                required property int index
                                canMoveUp: index > 0
                                canMoveDown: index < sourcesView.count - 1
                                width: sourcesView.width
                                onSelectedRequested: studioController.selectItem(itemId)
                                onRenameRequested: { root.renameSourceId = sourceId; renameSourceField.text = name; renameSourceDialog.open() }
                                onVisibleRequested: value => studioController.setItemVisible(itemId, value)
                                onLockedRequested: value => studioController.setItemLocked(itemId, value)
                                onMoveRequested: direction => studioController.moveSceneItem(itemId, direction)
                                onRemoveRequested: { root.removeSourceId = ""; root.removeItemId = itemId; removeSourceDialog.open() }
                                onMoveToRequested: targetIndex => studioController.moveSceneItemTo(itemId, targetIndex < 0 ? sourcesView.count - 1 : targetIndex)
                                onTransformRequested: action => {
                                    if (action === "edit") transformDialog.openFor(itemId, studioController.itemTransform(itemId))
                                    else studioController.applyTransformAction(itemId, action)
                                }
                                onPropertiesRequested: {
                                    root.propertiesSourceId = sourceId
                                    root.propertiesSourceType = type
                                    sourcePropertiesName.text = name
                                    sourcePropertiesDialog.open()
                                }
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
                                objectName: "previewCanvas"
                                anchors.centerIn: parent
                                width: Math.min(parent.width, parent.height * 16 / 9)
                                height: Math.round(width * 9 / 16)
                                color: "#000000"
                                border.color: "#48616A"
                                border.width: 1
                                clip: true
                                Item {
                                    id: programLayer
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    clip: true
                                    Repeater {
                                        model: studioController.sceneItemsModel
                                        delegate: Rectangle {
                                            id: previewItem
                                            required property string itemId
                                            required property string name
                                            required property string type
                                            required property bool itemVisible
                                            required property bool itemLocked
                                            required property bool selected
                                            required property bool visual
                                            required property int zOrder
                                            required property real programX
                                            required property real programY
                                            required property real programWidth
                                            required property real programHeight
                                            required property real programScaleX
                                            required property real programScaleY
                                            required property real programRotation
                                            required property real cropLeft
                                            required property real cropTop
                                            required property real cropRight
                                            required property real cropBottom
                                            required property bool flipHorizontal
                                            required property bool flipVertical
                                            x: (programX + cropLeft * programScaleX) / 1920 * programLayer.width
                                            y: (programY + cropTop * programScaleY) / 1080 * programLayer.height
                                            width: Math.max(0, (programWidth - cropLeft - cropRight) * programScaleX / 1920 * programLayer.width)
                                            height: Math.max(0, (programHeight - cropTop - cropBottom) * programScaleY / 1080 * programLayer.height)
                                            z: zOrder + 1
                                            visible: itemVisible && visual && width > 0 && height > 0
                                            rotation: programRotation
                                            transformOrigin: Item.Center
                                            transform: Scale { origin.x: previewItem.width / 2; origin.y: previewItem.height / 2; xScale: previewItem.flipHorizontal ? -1 : 1; yScale: previewItem.flipVertical ? -1 : 1 }
                                            radius: 2
                                            color: type === "Image" ? "#704251" : (type === "Text" ? "#4A4670" : "#284F58")
                                            border.color: selected ? root.accent : (itemLocked ? "#AAB5B7" : "#66858C")
                                            border.width: selected ? 2 : 1
                                            opacity: 0.92
                                            Text { anchors.centerIn: parent; width: Math.max(0, parent.width - 20); text: name; color: "#F2F7F7"; font.pixelSize: 13; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: studioController.selectItem(itemId) }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                StudioPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    RowLayout { anchors.fill: parent; spacing: 8
                        Rectangle { Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4; color: root.accent }
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
                            objectName: "mixerView"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 5
                            model: studioController.mixerModel
                            delegate: MixerChannel {
                                width: mixerView.width
                                onVolumeChangedByUser: value => studioController.setMixerVolume(channelId, value)
                                onMuteRequested: value => studioController.setMixerMuted(channelId, value)
                                onResetVolumeRequested: studioController.setMixerVolume(channelId, 1.0)
                                onRenameRequested: { root.renameSourceId = channelId; renameSourceField.text = name; renameSourceDialog.open() }
                                onRemoveRequested: { root.removeItemId = ""; root.removeSourceId = channelId; removeSourceDialog.open() }
                            }
                        }
                        Text { Layout.fillWidth: true; visible: mixerView.count === 0; text: "No audio channels. Add a microphone or desktop-audio placeholder."; color: "#829399"; font.pixelSize: 11; wrapMode: Text.WordWrap }
                    }
                }
                StudioPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 150
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8
                        StudioSectionHeader { title: "Transition"; subtitle: "Between scenes" }
                        StudioComboBox { Layout.fillWidth: true; model: ["Cut", "Fade"]; currentIndex: studioController.transitionType === "Cut" ? 0 : 1; onActivated: studioController.setTransitionType(currentText) }
                        RowLayout { Layout.fillWidth: true; visible: studioController.transitionType === "Fade"
                            Text { text: "Duration"; color: "#AAB9BC"; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Text { text: studioController.transitionDurationMs + " ms"; color: "#D5E0E1"; font.pixelSize: 11 }
                        }
                        StudioSlider { Layout.fillWidth: true; visible: studioController.transitionType === "Fade"; from: 50; to: 2000; stepSize: 50; value: studioController.transitionDurationMs; onMoved: studioController.setTransitionDurationMs(value) }
                    }
                }
                StudioPanel {
                    id: controlsPanel
                    objectName: "controlsPanel"
                    Layout.fillWidth: true
                    Layout.minimumHeight: controlsLayout.implicitHeight + contentMargin * 2
                    Layout.preferredHeight: Layout.minimumHeight
                    color: "#172529"
                    ColumnLayout {
                        id: controlsLayout
                        anchors.fill: parent
                        spacing: 8
                        StudioSectionHeader { title: "Controls"; subtitle: "Output is not connected" }
                        StudioButton { Layout.fillWidth: true; implicitHeight: 34; text: "Start streaming"; enabled: false; ToolTip.visible: hovered; ToolTip.text: "Output engine arrives in a later milestone." }
                        StudioButton { Layout.fillWidth: true; implicitHeight: 34; text: "Start recording"; enabled: false; ToolTip.visible: hovered; ToolTip.text: "Output engine arrives in a later milestone." }
                        StudioButton { objectName: "combinedOutputButton"; Layout.fillWidth: true; implicitHeight: 34; text: "Record + stream"; enabled: false; ToolTip.visible: hovered; ToolTip.text: "Output engine arrives in a later milestone." }
                    }
                }
            }
        }
    }

    StudioDialog {
        id: sceneDialog; modal: true; title: "Create scene"; standardButtons: Dialog.Ok | Dialog.Cancel; anchors.centerIn: parent
        onOpened: { sceneNameField.text = ""; sceneNameField.forceActiveFocus(); standardButton(Dialog.Ok).enabled = false }
        onAccepted: { if (sceneNameField.text.trim().length > 0) studioController.addScene(sceneNameField.text) }
        contentItem: ColumnLayout { implicitWidth: 320; spacing: 8
            Text { text: "Scene name"; color: "#B6C6C9"; font.pixelSize: 12 }
            StudioTextField { id: sceneNameField; Layout.fillWidth: true; placeholderText: "Gameplay"; selectByMouse: true; onTextChanged: sceneDialog.standardButton(Dialog.Ok).enabled = text.trim().length > 0 }
        }
    }
    StudioDialog {
        id: renameSceneDialog; modal: true; title: "Rename scene"; standardButtons: Dialog.Ok | Dialog.Cancel; anchors.centerIn: parent
        onOpened: { renameSceneField.forceActiveFocus(); renameSceneDialog.standardButton(Dialog.Ok).enabled = renameSceneField.text.trim().length > 0 }
        onAccepted: { if (renameSceneField.text.trim().length > 0) studioController.renameScene(root.renameSceneId, renameSceneField.text) }
        contentItem: StudioTextField { id: renameSceneField; placeholderText: "Scene name"; selectByMouse: true; implicitWidth: 300 }
    }
    StudioDialog {
        id: renameSourceDialog; modal: true; title: "Rename source"; standardButtons: Dialog.Ok | Dialog.Cancel; anchors.centerIn: parent
        onOpened: { renameSourceField.forceActiveFocus(); renameSourceDialog.standardButton(Dialog.Ok).enabled = renameSourceField.text.trim().length > 0 }
        onAccepted: { if (renameSourceField.text.trim().length > 0) studioController.renameSource(root.renameSourceId, renameSourceField.text) }
        contentItem: StudioTextField { id: renameSourceField; placeholderText: "Source name"; selectByMouse: true; implicitWidth: 300 }
    }
    StudioDialog {
        id: deleteSceneDialog; modal: true; title: "Delete scene?"; standardButtons: Dialog.Ok | Dialog.Cancel; anchors.centerIn: parent
        contentItem: Text { text: "This scene and its layout will be removed."; color: "#DCE6E7"; width: 280; wrapMode: Text.WordWrap }
        onAccepted: studioController.deleteScene(root.deleteSceneId)
    }
    StudioDialog {
        id: removeSourceDialog; modal: true; title: "Remove source?"; standardButtons: Dialog.Ok | Dialog.Cancel; anchors.centerIn: parent
        contentItem: Text { text: "This removes the source from the current scene."; color: "#DCE6E7"; width: 280; wrapMode: Text.WordWrap }
        onAccepted: {
            if (root.removeItemId.length > 0) studioController.removeSceneItem(root.removeItemId)
            else if (root.removeSourceId.length > 0) studioController.removeSourceFromActiveScene(root.removeSourceId)
            root.removeItemId = ""
            root.removeSourceId = ""
        }
    }
    StudioDialog {
        id: sourcePickerDialog
        title: "Add source"
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        onOpened: { sourceNameField.text = ""; sourcePickerDialog.standardButton(Dialog.Ok).enabled = false }
        onAccepted: studioController.addSource(root.selectedSourceType, sourceNameField.text)
        contentItem: ColumnLayout {
            implicitWidth: 390
            spacing: 10
            Text { text: "Choose a placeholder source. Capture and devices are not connected yet."; color: "#9BAEB2"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Repeater {
                model: ["Display Capture", "Window Capture", "Game Capture", "Webcam", "Microphone", "Desktop Audio", "Image", "Text"]
                delegate: StudioButton {
                    required property string modelData
                    Layout.fillWidth: true
                    implicitHeight: 34
                    checkable: true
                    checked: root.selectedSourceType === modelData
                    text: modelData
                    onClicked: { root.selectedSourceType = modelData; sourcePickerDialog.standardButton(Dialog.Ok).enabled = sourceNameField.text.trim().length > 0 }
                }
            }
            Text { text: "Coming later: Browser Source, Media Source"; color: "#72858A"; font.pixelSize: 11 }
            StudioTextField { id: sourceNameField; Layout.fillWidth: true; placeholderText: root.selectedSourceType.length > 0 ? root.selectedSourceType + " name" : "Choose a source type first"; enabled: root.selectedSourceType.length > 0; selectByMouse: true; onTextChanged: sourcePickerDialog.standardButton(Dialog.Ok).enabled = root.selectedSourceType.length > 0 && text.trim().length > 0 }
        }
    }
    StudioDialog {
        id: sourcePropertiesDialog; objectName: "sourcePropertiesDialog"; modal: true; title: "Source properties"; standardButtons: Dialog.Ok | Dialog.Cancel; anchors.centerIn: parent
        onOpened: sourcePropertiesName.forceActiveFocus()
        onAccepted: studioController.renameSource(root.propertiesSourceId, sourcePropertiesName.text)
        contentItem: ColumnLayout {
            implicitWidth: 320
            spacing: 8
            Text { text: root.propertiesSourceType + " placeholder"; color: "#829399"; font.pixelSize: 11 }
            Text { text: "Source name"; color: "#B6C6C9"; font.pixelSize: 12 }
            StudioTextField { id: sourcePropertiesName; Layout.fillWidth: true; selectByMouse: true }
        }
    }
    TransformDialog {
        id: transformDialog
        anchors.centerIn: parent
        onTransformAccepted: values => studioController.setItemTransform(sceneItemId, values)
    }
}
