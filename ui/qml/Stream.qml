import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Item {
    id: root
    objectName: "streamSetupPage"
    anchors.fill: parent
    property var destinations: studioController.streamingProviders()
    property string selectedProvider: studioController.activeStreamProvider
    property string connectionState: studioController.providerConnectionState(selectedProvider)
    property bool outputBusy: studioController.streamer.state === "Connecting"
                              || studioController.streamer.state === "Stopping"
                              || studioController.platformState === "Authenticating"
                              || studioController.platformState === "Creating Broadcast"
                              || studioController.platformState === "Creating Stream"
                              || studioController.platformState === "Connecting"
                              || studioController.platformState === "Waiting for YouTube"

    Connections {
        target: studioController
        function onProjectChanged() {
            root.destinations = studioController.streamingProviders()
            root.selectedProvider = studioController.activeStreamProvider
            root.connectionState = studioController.providerConnectionState(root.selectedProvider)
        }
        function onPlatformStateChanged() {
            root.destinations = studioController.streamingProviders()
            root.connectionState = studioController.providerConnectionState(root.selectedProvider)
        }
    }

    function formatElapsed(ms) {
        const seconds = Math.floor(ms / 1000)
        const hours = Math.floor(seconds / 3600)
        const minutes = Math.floor((seconds % 3600) / 60)
        const remainder = seconds % 60
        return (hours < 10 ? "0" : "") + hours + ":" + (minutes < 10 ? "0" : "") + minutes + ":" + (remainder < 10 ? "0" : "") + remainder
    }

    function saveSettings() {
        studioController.setStreamConfiguration(serverField.text, keyField.text,
                                                parseInt(videoBitrateField.text || "6000"),
                                                parseInt(audioBitrateField.text || "160"),
                                                parseInt(fpsBox.currentText || "60"))
    }

    function selectDestination(id) {
        const candidate = destinations.find(d => d.id === id)
        if (!candidate || candidate.provider === selectedProvider) return
        if (selectedProvider === "custom-rtmp") saveSettings()
        else if (selectedProvider === "youtube") youtubeControls.saveForm()
        else if (selectedProvider === "twitch") twitchControls.saveForm()
        else if (selectedProvider === "kick") kickControls.saveForm()
        studioController.selectStreamDestination(id)
    }

    function primaryAction() {
        if (selectedProvider === "custom-rtmp") {
            saveSettings()
            studioController.toggleStreaming()
        } else if (selectedProvider === "youtube") {
            youtubeControls.startOrStop()
        } else if (selectedProvider === "twitch") {
            twitchControls.startOrStop()
        } else if (selectedProvider === "kick") {
            kickControls.saveForm()
            studioController.updateKickChannel()
        }
    }

    Rectangle { anchors.fill: parent; color: "#0F171A" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text { text: "Stream Setup"; color: "#F5F8F8"; font.pixelSize: 22; font.weight: Font.DemiBold }
                Text { text: "Destination and broadcast settings"; color: "#829399"; font.pixelSize: 11 }
            }
            Rectangle {
                Layout.preferredWidth: 168
                Layout.preferredHeight: 30
                radius: 4
                color: root.connectionState === "Live" ? "#264437" : "#1B292E"
                border.color: root.connectionState === "Live" ? "#8AAC7D" : "#30434A"
                Text {
                    anchors.fill: parent
                    anchors.margins: 6
                    text: root.connectionState
                    color: root.connectionState === "Live" ? "#B7E65C" : "#D6E2E4"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            StudioPanel {
                id: destinationsPanel
                objectName: "streamDestinationsPanel"
                Layout.preferredWidth: 206
                Layout.minimumWidth: 170
                Layout.maximumWidth: 240
                Layout.fillHeight: true
                contentMargin: 10
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4
                    Text {
                        text: "Destinations"
                        color: "#EAF0F1"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        Layout.leftMargin: 5
                        Layout.bottomMargin: 6
                    }
                    Repeater {
                        model: root.destinations
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 55
                            radius: 4
                            color: modelData.selected ? "#25443A" : (hoverArea.containsMouse ? "#203139" : "transparent")
                            border.color: modelData.selected ? "#769877" : "transparent"
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 9
                                spacing: 7
                                Rectangle {
                                    Layout.preferredWidth: 7
                                    Layout.preferredHeight: 7
                                    radius: 4
                                    color: modelData.connectionState === "Live" || modelData.connected ? "#B7E65C" : "#70888D"
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text { Layout.fillWidth: true; text: modelData.label; color: "#EDF4F4"; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.accountName || modelData.connectionState
                                        color: "#9DB0B3"
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                            MouseArea {
                                id: hoverArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: root.selectDestination(modelData.id)
                                Accessible.name: "Select " + modelData.label
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            StudioPanel {
                id: settingsPanel
                objectName: "streamSettingsPanel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 300
                contentMargin: 16
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    StudioSectionHeader {
                        title: root.selectedProvider === "custom-rtmp" ? "Custom RTMP"
                               : root.selectedProvider === "youtube" ? "YouTube"
                               : root.selectedProvider === "twitch" ? "Twitch" : "Kick"
                        subtitle: root.selectedProvider === "custom-rtmp"
                                  ? "Server and encoding settings" : "Account and broadcast settings"
                    }
                    ScrollView {
                        id: customScroll
                        visible: root.selectedProvider === "custom-rtmp"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        contentWidth: availableWidth
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ColumnLayout {
                            width: customScroll.availableWidth - 4
                            spacing: 9
                            Text { text: "Server"; color: "#B6C6C9"; font.pixelSize: 12 }
                            StudioTextField { id: serverField; Layout.fillWidth: true; placeholderText: "rtmp://server/app"; text: studioController.streamServerUrl }
                            Text { text: "Stream Key"; color: "#B6C6C9"; font.pixelSize: 12 }
                            StudioTextField { id: keyField; Layout.fillWidth: true; placeholderText: "Paste stream key"; text: studioController.streamKey; echoMode: TextInput.Password }
                            Text { text: "The stream key stays in this session and is not saved in the Studio project."; color: "#8FA3A7"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                rowSpacing: 9
                                columnSpacing: 12
                                Text { text: "Video Bitrate"; color: "#B6C6C9"; font.pixelSize: 12 }
                                StudioTextField { id: videoBitrateField; Layout.fillWidth: true; text: studioController.streamVideoBitrateKbps.toString(); validator: IntValidator { bottom: 300; top: 50000 } inputMethodHints: Qt.ImhDigitsOnly }
                                Text { text: "Audio Bitrate"; color: "#B6C6C9"; font.pixelSize: 12 }
                                StudioTextField { id: audioBitrateField; Layout.fillWidth: true; text: studioController.streamAudioBitrateKbps.toString(); validator: IntValidator { bottom: 64; top: 512 } inputMethodHints: Qt.ImhDigitsOnly }
                                Text { text: "FPS"; color: "#B6C6C9"; font.pixelSize: 12 }
                                StudioComboBox { id: fpsBox; Layout.fillWidth: true; model: ["30", "60"]; currentIndex: studioController.streamFrameRate <= 30 ? 0 : 1 }
                            }
                            Item { Layout.preferredHeight: 4 }
                        }
                    }
                    YouTubeControls { id: youtubeControls; visible: root.selectedProvider === "youtube"; Layout.fillWidth: true; Layout.fillHeight: true }
                    TwitchControls { id: twitchControls; visible: root.selectedProvider === "twitch"; Layout.fillWidth: true; Layout.fillHeight: true }
                    KickControls { id: kickControls; visible: root.selectedProvider === "kick"; Layout.fillWidth: true; Layout.fillHeight: true }
                }
            }
        }

        StudioPanel {
            id: connectionPanel
            objectName: "streamConnectionPanel"
            Layout.fillWidth: true
            Layout.preferredHeight: 84
            contentMargin: 11
            ColumnLayout {
                anchors.fill: parent
                spacing: 3
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: root.selectedProvider === "custom-rtmp"
                              ? studioController.streamer.state
                              : root.connectionState
                        color: "#EAF0F1"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        text: root.formatElapsed(studioController.streamer.elapsedMs)
                        color: "#B7E65C"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                    StudioButton {
                        visible: root.selectedProvider === "custom-rtmp" && studioController.streamer.state === "Idle"
                        text: "Save"
                        implicitHeight: 32
                        onClicked: root.saveSettings()
                    }
                    StudioButton {
                        id: primaryButton
                        objectName: "streamPrimaryAction"
                        implicitHeight: 34
                        text: root.selectedProvider === "custom-rtmp"
                              ? (studioController.streamer.state === "Live" || studioController.streamer.state === "Connecting" || studioController.streamer.state === "Reconnecting" ? "Stop Streaming" : "Start Streaming")
                              : root.selectedProvider === "youtube"
                                ? (studioController.youtubeStreamActive ? "Stop Streaming" : youtubeControls.scheduled ? "Schedule Broadcast" : "Go Live")
                                : root.selectedProvider === "twitch"
                                  ? (studioController.twitchStreamActive ? "Stop Streaming" : "Go Live")
                                  : "Update Channel"
                        enabled: root.selectedProvider === "custom-rtmp"
                                 ? studioController.streamer.state !== "Stopping"
                                 : root.selectedProvider === "youtube"
                                   ? studioController.youtubeStreamActive || (root.connectionState === "Ready to stream" && !root.outputBusy)
                                   : root.selectedProvider === "twitch"
                                     ? studioController.twitchStreamActive || (root.connectionState === "Ready to stream" && !root.outputBusy)
                                     : root.connectionState === "Connected"
                        onClicked: root.primaryAction()
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: root.selectedProvider === "custom-rtmp"
                              ? (studioController.streamer.errorMessage || "RTMP output is offline.")
                              : root.connectionState === "Not configured"
                                ? "Configure this platform to connect an account."
                                : studioController.statusMessage
                        color: studioController.platformState === "Error" && root.selectedProvider !== "custom-rtmp" ? "#E7B17B" : "#8FA3A7"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        ToolTip.visible: statusHover.containsMouse && text.length > 0
                        ToolTip.text: text
                        MouseArea { id: statusHover; anchors.fill: parent; hoverEnabled: true }
                    }
                    Text {
                        text: "Bitrate " + (studioController.streamer.diagnostics.estimatedBitrateKbps || 0) + " Kbps"
                              + "   Dropped " + (studioController.streamer.diagnostics.droppedStreamFrames || 0)
                              + "   Reconnects " + (studioController.streamer.diagnostics.reconnectCount || 0)
                        color: "#8FA3A7"
                        font.pixelSize: 10
                    }
                }
            }
        }
    }
}
