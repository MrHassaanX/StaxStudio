import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Item {
    id: root
    objectName: "youtubeControls"
    property var categories: studioController.youtubeCategories
    property var broadcasts: studioController.youtubeBroadcasts
    property var metadata: studioController.activeStreamMetadata
    property var destinations: studioController.streamingProviders()
    property var destination: destinations.find(d => d.provider === "youtube")
    property bool connected: destination ? destination.connected : false
    property string accountName: destination ? destination.accountName : ""
    property var categoryNames: categories.map(c => c.title)
    property var broadcastNames: ["Create New Broadcast"].concat(broadcasts.map(b => b.title + " (" + b.status + ")"))
    property bool creatingBroadcast: broadcastBox.currentIndex <= 0

    function loadForm() {
        root.metadata = studioController.activeStreamMetadata
        titleField.text = metadata.title || ""
        descriptionField.text = metadata.description || ""
        privacyBox.currentIndex = Math.max(0, ["public", "unlisted", "private"].indexOf(metadata.privacy))
        categoryBox.currentIndex = categories.findIndex(c => c.id === metadata.categoryId)
        latencyBox.currentIndex = Math.max(0, ["normal", "low", "ultraLow"].indexOf(metadata.latency))
        kidsBox.currentIndex = metadata.madeForKids ? 1 : 0
        dvrBox.checked = metadata.enableDvr !== false
        autoStartBox.checked = metadata.autoStart !== false
        autoStopBox.checked = metadata.autoStop !== false
        scheduleBox.checked = metadata.scheduleForLater === true
        scheduleField.text = metadata.scheduledStartTime || ""
        thumbnailPath.text = metadata.thumbnailPath || ""
        let currentId = studioController.selectedYouTubeBroadcastId
        broadcastBox.currentIndex = Math.max(0, broadcasts.findIndex(b => b.id === currentId) + 1)
    }

    function saveForm() {
        let data = {
            title: titleField.text.trim(), description: descriptionField.text,
            privacy: ["public", "unlisted", "private"][privacyBox.currentIndex],
            categoryId: categoryBox.currentIndex >= 0 && categoryBox.currentIndex < categories.length ? categories[categoryBox.currentIndex].id : "",
            thumbnailPath: thumbnailPath.text,
            latency: ["normal", "low", "ultraLow"][latencyBox.currentIndex],
            madeForKids: kidsBox.currentIndex === 1, enableDvr: dvrBox.checked,
            autoStart: autoStartBox.checked, autoStop: autoStopBox.checked,
            scheduleForLater: scheduleBox.checked,
            scheduledStartTime: scheduleBox.checked ? scheduleField.text.trim() : ""
        }
        studioController.setActiveStreamMetadata(data)
    }

    Component.onCompleted: loadForm()
    onVisibleChanged: if (visible) Qt.callLater(root.loadForm)
    Connections {
        target: studioController
        function onProjectChanged() {
            root.destinations = studioController.streamingProviders()
            root.categories = studioController.youtubeCategories
            root.broadcasts = studioController.youtubeBroadcasts
        }
    }
    onCategoriesChanged: {
        if (visible && categoryBox.currentIndex < 0)
            categoryBox.currentIndex = categories.findIndex(c => c.id === root.metadata.categoryId)
    }

    FileDialog {
        id: thumbnailDialog
        title: "Select YouTube thumbnail"
        nameFilters: ["Images (*.jpg *.jpeg *.png)"]
        onAccepted: thumbnailPath.text = selectedFile.toString()
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scroll.availableWidth - 4
            spacing: 9

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: "Account"
                    color: "#EAF0F1"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }
                StudioButton {
                    text: "Refresh"
                    visible: root.connected
                    onClicked: studioController.refreshYouTubeOptions()
                }
            }
            Text {
                Layout.fillWidth: true
                text: root.connected ? root.accountName : "YouTube is not connected"
                color: root.connected ? "#B7E65C" : "#8FA3A7"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
            RowLayout {
                Layout.fillWidth: true
                StudioButton {
                    text: root.connected ? "Reconnect" : "Connect YouTube"
                    onClicked: studioController.connectYouTube()
                }
                StudioButton {
                    visible: root.connected
                    text: "Disconnect"
                    onClicked: studioController.disconnectYouTube()
                }
                StudioButton {
                    visible: studioController.platformState === "Authenticating"
                    text: "Cancel"
                    onClicked: studioController.cancelYouTubeConnection()
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.topMargin: 6; height: 1; color: "#34474A" }
            Text { text: "Broadcast"; color: "#EAF0F1"; font.pixelSize: 14; font.weight: Font.DemiBold }
            StudioComboBox {
                id: broadcastBox
                Layout.fillWidth: true
                model: root.broadcastNames
                onActivated: function(index) {
                    studioController.selectYouTubeBroadcast(index === 0 ? "" : root.broadcasts[index - 1].id)
                    root.loadForm()
                }
            }
            Text {
                visible: !root.creatingBroadcast
                Layout.fillWidth: true
                text: "Existing broadcast details are managed on YouTube. StaxStudio will use the selected broadcast unchanged."
                color: "#93A8AC"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
            Text { text: "Title"; color: "#B6C6C9"; font.pixelSize: 12 }
            StudioTextField { id: titleField; Layout.fillWidth: true; enabled: root.creatingBroadcast; placeholderText: "Name your live broadcast" }
            Text { text: "Description"; color: "#B6C6C9"; font.pixelSize: 12 }
            TextArea {
                id: descriptionField
                enabled: root.creatingBroadcast
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                color: "#E5EEEE"
                font.pixelSize: 12
                wrapMode: TextEdit.Wrap
                background: Rectangle { radius: 4; color: "#1B292E"; border.color: descriptionField.activeFocus ? "#6C8F7D" : "#344850" }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Privacy"; color: "#B6C6C9"; font.pixelSize: 12 }
                    StudioComboBox { id: privacyBox; Layout.fillWidth: true; enabled: root.creatingBroadcast; model: ["Public", "Unlisted", "Private"] }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Category"; color: "#B6C6C9"; font.pixelSize: 12 }
                    StudioComboBox { id: categoryBox; Layout.fillWidth: true; enabled: root.creatingBroadcast; model: root.categoryNames }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Made for Kids"; color: "#B6C6C9"; font.pixelSize: 12 }
                    StudioComboBox { id: kidsBox; Layout.fillWidth: true; enabled: root.creatingBroadcast; model: ["No", "Yes"] }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: "Latency"; color: "#B6C6C9"; font.pixelSize: 12 }
                    StudioComboBox { id: latencyBox; Layout.fillWidth: true; enabled: root.creatingBroadcast; model: ["Normal", "Low", "Ultra Low"] }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                StudioCheckBox { id: dvrBox; text: "Enable DVR"; enabled: root.creatingBroadcast }
                StudioCheckBox { id: autoStartBox; text: "Auto Start"; enabled: root.creatingBroadcast }
                StudioCheckBox { id: autoStopBox; text: "Auto Stop"; enabled: root.creatingBroadcast }
            }
            StudioCheckBox { id: scheduleBox; text: "Schedule for Later"; enabled: root.creatingBroadcast }
            StudioTextField {
                id: scheduleField
                enabled: root.creatingBroadcast
                visible: scheduleBox.checked
                Layout.fillWidth: true
                placeholderText: "YYYY-MM-DDTHH:MM:SSZ (UTC)"
            }

            Rectangle { Layout.fillWidth: true; Layout.topMargin: 6; height: 1; color: "#34474A" }
            Text { text: "Thumbnail"; color: "#EAF0F1"; font.pixelSize: 14; font.weight: Font.DemiBold }
            RowLayout {
                Layout.fillWidth: true
                Image {
                    Layout.preferredWidth: 112; Layout.preferredHeight: 63
                    visible: thumbnailPath.text.length > 0
                    source: thumbnailPath.text
                    fillMode: Image.PreserveAspectFit
                }
                Text {
                    id: thumbnailPath
                    Layout.fillWidth: true
                    text: ""
                    color: "#93A8AC"
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                }
                StudioButton { text: thumbnailPath.text ? "Replace" : "Choose"; onClicked: thumbnailDialog.open() }
                StudioButton { visible: thumbnailPath.text.length > 0; text: "Remove"; onClicked: thumbnailPath.text = "" }
            }
            StudioCheckBox { id: recordBox; text: "Record Stream" }
            RowLayout {
                Layout.fillWidth: true
                StudioButton { text: "Save"; visible: root.creatingBroadcast; onClicked: root.saveForm() }
                Item { Layout.fillWidth: true }
                StudioButton {
                    text: studioController.youtubeStreamActive ? "Stop Streaming" : (scheduleBox.checked && broadcastBox.currentIndex === 0 ? "Schedule Broadcast" : "Go Live")
                    enabled: studioController.youtubeStreamActive || (root.connected && studioController.platformState !== "Authenticating"
                             && studioController.platformState !== "Creating Broadcast"
                             && studioController.platformState !== "Creating Stream"
                             && studioController.platformState !== "Connecting"
                             && studioController.platformState !== "Waiting for YouTube")
                    onClicked: {
                        if (studioController.youtubeStreamActive) studioController.stopYouTubeStream()
                        else { root.saveForm(); studioController.goLiveYouTube(recordBox.checked) }
                    }
                }
            }
            Item { Layout.preferredHeight: 8 }
        }
    }
}
