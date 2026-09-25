import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import StaxStudio.Render

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
    property var propertyTargets: []
    property var propertyFormats: []
    property var propertyConfiguration: ({})
    property string pendingPreviewTransformItemId: ""
    property var pendingPreviewTransform: ({})
    function formatElapsed(ms) {
        const seconds = Math.floor(ms / 1000)
        const hours = Math.floor(seconds / 3600)
        const minutes = Math.floor((seconds % 3600) / 60)
        const remainder = seconds % 60
        return (hours < 10 ? "0" : "") + hours + ":" + (minutes < 10 ? "0" : "") + minutes + ":" + (remainder < 10 ? "0" : "") + remainder
    }
    function queuePreviewTransform(itemId, transform) {
        pendingPreviewTransformItemId = itemId
        pendingPreviewTransform = transform
        previewTransformTimer.restart()
    }
    function commitQueuedPreviewTransform() {
        if (previewTransformTimer.running) {
            previewTransformTimer.stop()
            studioController.previewItemTransform(pendingPreviewTransformItemId, pendingPreviewTransform)
        }
        pendingPreviewTransformItemId = ""
        pendingPreviewTransform = ({})
        studioController.commitPreviewTransform()
    }

    // Mouse movement can arrive much faster than display refresh. Coalescing
    // keeps the compositor responsive while persistence remains release-only.
    Timer {
        id: previewTransformTimer
        interval: 16
        repeat: false
        onTriggered: studioController.previewItemTransform(root.pendingPreviewTransformItemId, root.pendingPreviewTransform)
    }

    Rectangle { anchors.fill: parent; color: "#0F171A" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

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
            IconButton {
                iconName: "settings"; tooltip: "Workspace layout"
                onClicked: layoutMenu.popup()
                StudioMenu {
                    id: layoutMenu
                    StudioMenuItem { text: "Reset Layout"; onTriggered: studioController.dockLayout.reset() }
                    StudioMenuItem { text: studioController.dockLayout.locked ? "Unlock Layout" : "Lock Layout"; onTriggered: studioController.dockLayout.locked = !studioController.dockLayout.locked }
                    MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: "#31454B" } }
                    Repeater {
                        model: [{key:"scenes",label:"Scenes"},{key:"sources",label:"Sources"},{key:"mixer",label:"Audio Mixer"},{key:"transition",label:"Transitions"},{key:"controls",label:"Controls"}]
                        delegate: StudioMenuItem {
                            required property var modelData
                            readonly property bool panelHidden: studioController.dockLayout.hiddenPanels.indexOf(modelData.key)>=0
                            text: (panelHidden ? "Show " : "Hide ") + modelData.label
                            enabled: !studioController.dockLayout.locked
                            onTriggered: studioController.dockLayout.setPanelVisible(modelData.key,panelHidden)
                        }
                    }
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

        ScrollView {
            id: workspaceScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: Math.max(availableWidth, workspace.minimumLayout.minimumWidth)
            contentHeight: Math.max(availableHeight, workspace.minimumLayout.minimumHeight)
            DockWorkspace {
                id: workspace
                objectName: "studioWorkspace"
                width: workspaceScroll.contentWidth
                height: workspaceScroll.contentHeight
                manager: studioController.dockLayout
                DockPanel {
                    id: scenesPanel
                    dockId: "scenes"; dockTitle: "Scenes"; workspace: workspace
                    objectName: "scenesPanel"
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10
                        StudioSectionHeader {
                            title: ""; subtitle: "Program layouts"
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
                DockPanel {
                    id: sourcesPanel
                    dockId: "sources"; dockTitle: "Sources"; workspace: workspace
                    objectName: "sourcesPanel"
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10
                        StudioSectionHeader {
                            title: ""; subtitle: "Layers in " + studioController.activeSceneName
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
                                    root.propertyConfiguration = studioController.sourceConfiguration(sourceId)
                                    root.propertyTargets = studioController.captureTargets(type)
                                    root.propertyFormats = type === "Webcam" ? studioController.cameraFormats(root.propertyConfiguration.targetId || "") : []
                                    sourcePropertiesName.text = name
                                    sourcePropertiesDialog.open()
                                }
                            }
                        }
                        Text { Layout.fillWidth: true; visible: sourcesView.count === 0; text: "Add a source to start composing this scene."; color: "#829399"; font.pixelSize: 11; wrapMode: Text.WordWrap }
                    }
                }
            ColumnLayout {
                objectName: "topWorkspace"
                property var placement: workspace.panelRect("preview")
                x: placement.x; y: placement.y; width: placement.width; height: placement.height
                spacing: 8
                StudioPanel {
                    objectName: "previewPanel"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    contentMargin: 14
                    color: "#131E22"
                    border.color: "#30434A"
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: studioController.activeSceneName; color: "#F3F7F7"; font.pixelSize: 14; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            StudioComboBox { objectName: "previewScaleMode"; Layout.preferredWidth: 82; Layout.preferredHeight: 28; model: ["Fit"]; currentIndex: 0 }
                            Text { text: Math.round(previewCanvas.width / studioController.programWidth * 100) + "%"; color: "#8FA4A8"; font.pixelSize: 10; font.weight: Font.DemiBold }
                            Text { text: "PROGRAM PREVIEW"; color: "#8FA4A8"; font.pixelSize: 10; font.weight: Font.DemiBold }
                        }
                        Item {
                            id: previewFrame
                            Layout.fillWidth: true; Layout.fillHeight: true
                            objectName: "previewWorkspace"
                            readonly property real canvasInset: width < 600 ? 16 : 24
                            clip: true
                            Rectangle {
                                id: previewCanvas
                                objectName: "previewCanvas"
                                property rect presentation: { const pw=studioController.programWidth; const ph=studioController.programHeight; return studioController.previewCanvasRect(previewFrame.width, previewFrame.height, previewFrame.canvasInset) }
                                x: presentation.x; y: presentation.y; width: presentation.width; height: presentation.height
                                color: "#000000"
                                border.color: "#48616A"
                                border.width: 1
                                clip: true
                                Loader {
                                    id: programPreviewLoader
                                    objectName: "programPreview"
                                    anchors.fill: parent
                                    active: appController.gpuPreviewEnabled
                                    sourceComponent: programPreviewComponent
                                }
                                Component {
                                    id: programPreviewComponent
                                    ProgramPreview {
                                        engine: studioController.programEngine
                                    }
                                }
                                Text {
                                    anchors.centerIn: parent
                                    width: parent.width - 32
                                    visible: programPreviewLoader.item
                                        && programPreviewLoader.item.rendererState.indexOf("failed") >= 0
                                    text: "GPU preview unavailable"
                                    color: "#E7B17B"
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                }
                                Item {
                                    id: programLayer
                                    anchors.fill: parent
                                    clip: true
                                    Repeater {
                                        model: studioController.sceneItemsModel
                                        delegate: Item {
                                            id: editorItem
                                            objectName: "editorOverlay_" + itemId
                                            required property string itemId
                                            required property string sourceId
                                            required property string name
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
                                            x: (programX + cropLeft * programScaleX) / studioController.programWidth * programLayer.width
                                            y: (programY + cropTop * programScaleY) / studioController.programHeight * programLayer.height
                                            width: Math.max(0, (programWidth - cropLeft - cropRight) * programScaleX / studioController.programWidth * programLayer.width)
                                            height: Math.max(0, (programHeight - cropTop - cropBottom) * programScaleY / studioController.programHeight * programLayer.height)
                                            z: zOrder + 1
                                            visible: itemVisible && visual && width > 0 && height > 0
                                            rotation: programRotation
                                            transformOrigin: Item.Center
                                            Rectangle { anchors.fill: parent; color: "transparent"; border.color: root.accent; border.width: 2; visible: editorItem.selected }
                                            Text { visible: editorItem.selected; anchors.left: parent.left; anchors.bottom: parent.top; text: editorItem.name; color: "#EAF4E6"; font.pixelSize: 10; font.weight: Font.DemiBold }
                                            MouseArea {
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                cursorShape: editorItem.itemLocked ? Qt.ArrowCursor : Qt.SizeAllCursor
                                                property point pressPoint
                                                property var startTransform
                                                onPressed: event => {
                                                    studioController.selectItem(editorItem.itemId)
                                                    if (event.button === Qt.RightButton) { canvasSourceMenu.open(); return }
                                                    if (editorItem.itemLocked) return
                                                    pressPoint = mapToItem(programLayer, event.x, event.y)
                                                    startTransform = studioController.itemTransform(editorItem.itemId)
                                                }
                                                onPositionChanged: event => {
                                                    if (editorItem.itemLocked || !pressed || !startTransform) return
                                                    const next = studioController.itemTransform(editorItem.itemId)
                                                    const position = mapToItem(programLayer, event.x, event.y)
                                                    next.x = startTransform.x + (position.x - pressPoint.x) / programLayer.width * studioController.programWidth
                                                    next.y = startTransform.y + (position.y - pressPoint.y) / programLayer.height * studioController.programHeight
                                                    root.queuePreviewTransform(editorItem.itemId, next)
                                                }
                                                onReleased: if (!editorItem.itemLocked) root.commitQueuedPreviewTransform()
                                            }
                                            StudioMenu {
                                                id: canvasSourceMenu
                                                StudioMenuItem { text: "Properties"; onTriggered: { root.propertiesSourceId = editorItem.sourceId; root.propertyConfiguration = studioController.sourceConfiguration(editorItem.sourceId); root.propertyTargets = studioController.captureTargets(editorItem.type); sourcePropertiesDialog.open() } }
                                                StudioMenuItem { text: "Rename"; onTriggered: { root.renameSourceId = editorItem.sourceId; renameSourceField.text = editorItem.name; renameSourceDialog.open() } }
                                                StudioMenuItem { text: editorItem.itemVisible ? "Hide source" : "Show source"; onTriggered: studioController.setItemVisible(editorItem.itemId, !editorItem.itemVisible) }
                                                StudioMenuItem { text: editorItem.itemLocked ? "Unlock source" : "Lock source"; onTriggered: studioController.setItemLocked(editorItem.itemId, !editorItem.itemLocked) }
                                                StudioMenu {
                                                    title: "Order"
                                                    StudioMenuItem { text: "Move to top"; onTriggered: studioController.moveSceneItemTo(editorItem.itemId, 0) }
                                                    StudioMenuItem { text: "Move up"; onTriggered: studioController.moveSceneItem(editorItem.itemId, -1) }
                                                    StudioMenuItem { text: "Move down"; onTriggered: studioController.moveSceneItem(editorItem.itemId, 1) }
                                                    StudioMenuItem { text: "Move to bottom"; onTriggered: studioController.moveSceneItemTo(editorItem.itemId, studioController.sceneItemsModel.rowCount() - 1) }
                                                }
                                                StudioMenu {
                                                    title: "Transform"
                                                    StudioMenuItem { text: "Edit Transform"; onTriggered: { transformDialog.openFor(editorItem.itemId, studioController.itemTransform(editorItem.itemId)) } }
                                                    StudioMenuItem { text: "Reset Transform"; onTriggered: studioController.applyTransformAction(editorItem.itemId, "reset") }
                                                    StudioMenuItem { text: "Fit to Canvas"; onTriggered: studioController.applyTransformAction(editorItem.itemId, "fit") }
                                                    StudioMenuItem { text: "Stretch to Canvas"; onTriggered: studioController.applyTransformAction(editorItem.itemId, "stretch") }
                                                    StudioMenuItem { text: "Center"; onTriggered: studioController.applyTransformAction(editorItem.itemId, "center") }
                                                    StudioMenuItem { text: "Center Horizontally"; onTriggered: studioController.applyTransformAction(editorItem.itemId, "centerHorizontal") }
                                                    StudioMenuItem { text: "Center Vertically"; onTriggered: studioController.applyTransformAction(editorItem.itemId, "centerVertical") }
                                                    StudioMenuItem { text: "Rotate 90 CW"; onTriggered: studioController.applyTransformAction(editorItem.itemId, "rotate90Clockwise") }
                                                    StudioMenuItem { text: "Rotate 90 CCW"; onTriggered: studioController.applyTransformAction(editorItem.itemId, "rotate90CounterClockwise") }
                                                    StudioMenuItem { text: "Rotate 180"; onTriggered: studioController.applyTransformAction(editorItem.itemId, "rotate180") }
                                                    StudioMenuItem { text: "Flip Horizontal"; onTriggered: studioController.applyTransformAction(editorItem.itemId, "flipHorizontal") }
                                                    StudioMenuItem { text: "Flip Vertical"; onTriggered: studioController.applyTransformAction(editorItem.itemId, "flipVertical") }
                                                }
                                                StudioMenuItem { text: "Remove"; destructive: true; onTriggered: { root.removeItemId = editorItem.itemId; removeSourceDialog.open() } }
                                            }
                                            Rectangle {
                                                visible: editorItem.selected && !editorItem.itemLocked
                                                width: 10; height: 10; radius: 2; color: root.accent; border.color: "#122024"
                                                anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: -5
                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.SizeFDiagCursor
                                                    property point pressPoint
                                                    property var startTransform
                                                    onPressed: event => { pressPoint = mapToItem(programLayer, event.x, event.y); startTransform = studioController.itemTransform(editorItem.itemId) }
                                                    onPositionChanged: event => {
                                                        if (!pressed || !startTransform) return
                                                        const next = studioController.itemTransform(editorItem.itemId)
                                                        const position = mapToItem(programLayer, event.x, event.y)
                                                        const widthDelta = (position.x - pressPoint.x) / programLayer.width * studioController.programWidth
                                                        next.width = Math.max(1, startTransform.width + widthDelta / Math.max(0.01, startTransform.scaleX))
                                                        next.height = Math.max(1, startTransform.height * next.width / Math.max(1, startTransform.width))
                                                        root.queuePreviewTransform(editorItem.itemId, next)
                                                    }
                                                    onReleased: root.commitQueuedPreviewTransform()
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                StudioPanel {
                    Layout.fillWidth: true; Layout.preferredHeight: 46
                    RowLayout { anchors.fill: parent; spacing: 8
                        Rectangle { Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4; color: root.accent }
                        Text { Layout.fillWidth: true; text: studioController.statusMessage; color: "#AEBEC1"; font.pixelSize: 11; elide: Text.ElideRight }
                        Text { text: "Saved locally"; color: root.accent; font.pixelSize: 11; font.weight: Font.DemiBold }
                    }
                }
            }
                DockPanel {
                    id: mixerPanel
                    dockId: "mixer"; dockTitle: "Audio Mixer"; workspace: workspace
                    objectName: "mixerPanel"
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10
                        StudioSectionHeader { title: ""; subtitle: "Input levels" }
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
                        Text { Layout.fillWidth: true; visible: mixerView.count === 0; text: "No audio sources"; color: "#829399"; font.pixelSize: 11; wrapMode: Text.WordWrap }
                    }
                }
                DockPanel {
                    id: transitionPanel
                    dockId: "transition"; dockTitle: "Scene Transitions"; workspace: workspace
                    objectName: "transitionPanel"
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8
                        StudioSectionHeader { title: ""; subtitle: "Between scenes" }
                        StudioComboBox { Layout.fillWidth: true; model: ["Cut", "Fade"]; currentIndex: studioController.transitionType === "Cut" ? 0 : 1; onActivated: studioController.setTransitionType(currentText) }
                        RowLayout { Layout.fillWidth: true; visible: studioController.transitionType === "Fade"
                            Text { text: "Duration"; color: "#AAB9BC"; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Text { text: studioController.transitionDurationMs + " ms"; color: "#D5E0E1"; font.pixelSize: 11 }
                        }
                        StudioSlider { Layout.fillWidth: true; visible: studioController.transitionType === "Fade"; from: 50; to: 2000; stepSize: 50; value: studioController.transitionDurationMs; onMoved: studioController.setTransitionDurationMs(value) }
                    }
                }
                DockPanel {
                    id: controlsPanel
                    dockId: "controls"; dockTitle: "Controls"; workspace: workspace
                    objectName: "controlsPanel"
                    color: "#172529"
                    ColumnLayout {
                        id: controlsLayout
                        anchors.fill: parent
                        spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            StudioSectionHeader { Layout.fillWidth: true; title: ""; subtitle: studioController.recorder.state === "Recording" ? "Recording locally" : "Local recording" }
                            Text { visible: studioController.recorder.state === "Recording" || studioController.recorder.state === "Stopping"; text: "REC  " + root.formatElapsed(studioController.recorder.elapsedMs); color: root.accent; font.pixelSize: 11; font.weight: Font.DemiBold }
                        }
                        StudioButton { objectName: "recordingButton"; Layout.fillWidth: true; implicitHeight: 34; text: studioController.recorder.state === "Recording" ? "Stop recording" : "Start recording"; enabled: studioController.recorder.state !== "Starting" && studioController.recorder.state !== "Stopping"; onClicked: studioController.toggleRecording(); ToolTip.visible: hovered; ToolTip.text: studioController.recorder.state === "Error" ? studioController.recorder.errorMessage : "Record the program locally as an MKV file." }
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
        onOpened: { sourceNameField.text = ""; sourcePickerDialog.standardButton(Dialog.Ok).enabled = root.selectedSourceType.length > 0 }
        onAccepted: studioController.addSource(root.selectedSourceType, sourceNameField.text)
        contentItem: ColumnLayout {
            implicitWidth: 390
            spacing: 10
            Text { text: "Choose a placeholder source. Capture and devices are not connected yet."; color: "#9BAEB2"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Repeater {
                model: ["Display Capture", "Window Capture", "Game Capture", "Webcam", "Color Source", "Microphone", "Desktop Audio", "Image", "Text"]
                delegate: StudioButton {
                    required property string modelData
                    Layout.fillWidth: true
                    implicitHeight: 34
                    checkable: true
                    checked: root.selectedSourceType === modelData
                    text: modelData
                    onClicked: { root.selectedSourceType = modelData; sourceNameField.text = modelData; sourcePickerDialog.standardButton(Dialog.Ok).enabled = true }
                }
            }
            Text { text: "Coming later: Browser Source, Media Source"; color: "#72858A"; font.pixelSize: 11 }
            StudioTextField { id: sourceNameField; Layout.fillWidth: true; placeholderText: root.selectedSourceType.length > 0 ? root.selectedSourceType + " name" : "Choose a source type first"; enabled: root.selectedSourceType.length > 0; selectByMouse: true; onTextChanged: sourcePickerDialog.standardButton(Dialog.Ok).enabled = root.selectedSourceType.length > 0 }
        }
    }
    StudioDialog {
        id: sourcePropertiesDialog
        objectName: "sourcePropertiesDialog"
        modal: true
        title: "Source properties"
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        onOpened: {
            sourcePropertiesName.forceActiveFocus()
            sourceTarget.currentIndex = root.propertyTargets.map(function(item) { return item.id }).indexOf(root.propertyConfiguration.targetId || "")
            if (sourceTarget.currentIndex < 0 && root.propertyTargets.length > 0) sourceTarget.currentIndex = 0
            sourceFormat.currentIndex = root.propertyFormats.map(function(item) { return item.id }).indexOf(root.propertyConfiguration.formatId || "")
            if (sourceFormat.currentIndex < 0 && root.propertyFormats.length > 0) sourceFormat.currentIndex = 0
        }
        onAccepted: {
            studioController.renameSource(root.propertiesSourceId, sourcePropertiesName.text)
            if (root.propertyTargets.length > 0) {
                const target = root.propertyTargets[sourceTarget.currentIndex]
                const format = root.propertyFormats.length > 0 ? root.propertyFormats[sourceFormat.currentIndex] : null
                studioController.configureCaptureSource(root.propertiesSourceId, target ? target.id : "", format ? format.id : "", cursorCheck.checked)
            }
        }
        contentItem: ColumnLayout {
            implicitWidth: 360
            spacing: 10
            Text { text: root.propertiesSourceType; color: "#829399"; font.pixelSize: 11 }
            Text { text: "Source name"; color: "#B6C6C9"; font.pixelSize: 12 }
            StudioTextField { id: sourcePropertiesName; Layout.fillWidth: true; selectByMouse: true }
            Text { visible: root.propertyTargets.length > 0; text: root.propertiesSourceType === "Webcam" || root.propertiesSourceType === "Microphone" ? "Device" : (root.propertiesSourceType === "Desktop Audio" ? "Output device" : (root.propertiesSourceType === "Display Capture" ? "Display" : "Window")); color: "#B6C6C9"; font.pixelSize: 12 }
            StudioComboBox {
                id: sourceTarget
                visible: root.propertyTargets.length > 0
                Layout.fillWidth: true
                model: root.propertyTargets.map(function(item) { return item.name + (item.detail ? " - " + item.detail : "") })
                onActivated: {
                    if (root.propertiesSourceType === "Webcam") {
                        const target = root.propertyTargets[currentIndex]
                        root.propertyFormats = target ? studioController.cameraFormats(target.id) : []
                        sourceFormat.currentIndex = 0
                    }
                }
            }
            Text { visible: root.propertiesSourceType === "Webcam" && root.propertyFormats.length > 0; text: "Video format"; color: "#B6C6C9"; font.pixelSize: 12 }
            CheckBox {
                id: cursorCheck
                visible: root.propertiesSourceType === "Display Capture" || root.propertiesSourceType === "Window Capture" || root.propertiesSourceType === "Game Capture"
                text: "Capture cursor"
                checked: root.propertyConfiguration.captureCursor !== false
                contentItem: Text { text: cursorCheck.text; leftPadding: cursorCheck.indicator.width + 8; color: "#C8D5D7"; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter }
                indicator: Rectangle { implicitWidth: 16; implicitHeight: 16; x: cursorCheck.leftPadding; y: parent.height / 2 - height / 2; radius: 3; color: cursorCheck.checked ? "#7FAE70" : "#172328"; border.color: "#526B72"; Text { anchors.centerIn: parent; visible: cursorCheck.checked; text: "✓"; color: "#102016"; font.pixelSize: 12; font.weight: Font.Bold } }
            }
            StudioComboBox {
                id: sourceFormat
                visible: root.propertiesSourceType === "Webcam" && root.propertyFormats.length > 0
                Layout.fillWidth: true
                model: root.propertyFormats.map(function(item) { return item.name })
            }
            Text { visible: root.propertyTargets.length === 0; text: "This source has no device settings yet."; color: "#829399"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
    TransformDialog {
        id: transformDialog
        anchors.centerIn: parent
        onTransformAccepted: values => studioController.setItemTransform(sceneItemId, values)
    }
}
