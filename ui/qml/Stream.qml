import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Item {
    id: root
    anchors.fill: parent

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

    Rectangle { anchors.fill: parent; color: "#0F171A" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text { text: "Stream Setup"; color: "#F5F8F8"; font.pixelSize: 24; font.weight: Font.DemiBold }
                Text { text: "Configure a Custom RTMP destination for the program output."; color: "#829399"; font.pixelSize: 12 }
            }
            Rectangle {
                Layout.preferredWidth: 170
                Layout.preferredHeight: 38
                radius: 5
                color: studioController.streamer.state === "Live" ? "#2E4D3B" : "#172328"
                border.color: studioController.streamer.state === "Live" ? "#8AAC7D" : "#2F4148"
                Row {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: studioController.streamer.state === "Live" ? "#B7E65C" : "#73878D"
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: studioController.streamer.state === "Live" ? "LIVE  " + root.formatElapsed(studioController.streamer.elapsedMs) : studioController.streamer.state
                        color: "#EAF0F1"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            StudioPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentMargin: 18
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12
                    StudioSectionHeader { title: "Custom RTMP"; subtitle: "Server URL is saved. Stream key stays session-only for now." }

                    Text { text: "Server"; color: "#B6C6C9"; font.pixelSize: 12 }
                    StudioTextField {
                        id: serverField
                        Layout.fillWidth: true
                        placeholderText: "rtmp://server/app"
                        text: studioController.streamServerUrl
                    }

                    Text { text: "Stream Key"; color: "#B6C6C9"; font.pixelSize: 12 }
                    StudioTextField {
                        id: keyField
                        Layout.fillWidth: true
                        placeholderText: "Paste stream key"
                        text: studioController.streamKey
                        echoMode: TextInput.Password
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 10
                        columnSpacing: 12
                        Text { text: "Video Bitrate"; color: "#B6C6C9"; font.pixelSize: 12 }
                        StudioTextField {
                            id: videoBitrateField
                            Layout.fillWidth: true
                            text: studioController.streamVideoBitrateKbps.toString()
                            validator: IntValidator { bottom: 300; top: 50000 }
                            inputMethodHints: Qt.ImhDigitsOnly
                        }
                        Text { text: "Audio Bitrate"; color: "#B6C6C9"; font.pixelSize: 12 }
                        StudioTextField {
                            id: audioBitrateField
                            Layout.fillWidth: true
                            text: studioController.streamAudioBitrateKbps.toString()
                            validator: IntValidator { bottom: 64; top: 512 }
                            inputMethodHints: Qt.ImhDigitsOnly
                        }
                        Text { text: "FPS"; color: "#B6C6C9"; font.pixelSize: 12 }
                        StudioComboBox {
                            id: fpsBox
                            Layout.fillWidth: true
                            model: ["30", "60"]
                            currentIndex: studioController.streamFrameRate <= 30 ? 0 : 1
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Item { Layout.fillWidth: true }
                        StudioButton { text: "Save Settings"; onClicked: root.saveSettings() }
                        StudioButton {
                            text: studioController.streamer.state === "Live" || studioController.streamer.state === "Connecting" || studioController.streamer.state === "Reconnecting"
                                  ? "Stop Streaming" : "Start Streaming"
                            enabled: studioController.streamer.state !== "Stopping"
                            onClicked: { root.saveSettings(); studioController.toggleStreaming() }
                        }
                    }
                }
            }

            StudioPanel {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                contentMargin: 18
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    StudioSectionHeader { title: "Status"; subtitle: studioController.streamer.state === "Live" ? "RTMP output is live" : "RTMP output is offline" }
                    Text { Layout.fillWidth: true; text: studioController.streamer.state; color: "#EAF0F1"; font.pixelSize: 18; font.weight: Font.DemiBold }
                    Text { Layout.fillWidth: true; text: studioController.streamer.state === "Live" ? root.formatElapsed(studioController.streamer.elapsedMs) : "00:00:00"; color: "#B7E65C"; font.pixelSize: 13; font.weight: Font.DemiBold }
                    Text {
                        Layout.fillWidth: true
                        visible: studioController.streamer.errorMessage.length > 0
                        text: studioController.streamer.errorMessage
                        color: "#E7B17B"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "Bitrate: " + (studioController.streamer.diagnostics.estimatedBitrateKbps || 0) + " Kbps"
                        color: "#9BAEB2"
                        font.pixelSize: 11
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "Dropped frames: " + (studioController.streamer.diagnostics.droppedStreamFrames || 0)
                        color: "#9BAEB2"
                        font.pixelSize: 11
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "Reconnects: " + (studioController.streamer.diagnostics.reconnectCount || 0)
                        color: "#9BAEB2"
                        font.pixelSize: 11
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
