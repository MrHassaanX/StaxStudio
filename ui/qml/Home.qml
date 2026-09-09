import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Item {
    id: root

    anchors.fill: parent

    Column {
        id: content

        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: 52
        }
        spacing: 0

        Text {
            text: "Create without the clutter"
            color: "#13202A"
            font.pixelSize: 32
            font.weight: Font.DemiBold
        }

        Item {
            width: 1
            height: 10
        }

        Text {
            text: "Choose a workflow, then keep the controls close to the work."
            color: "#60717A"
            font.pixelSize: 15
        }

        Item {
            width: 1
            height: 28
        }

        RowLayout {
            width: parent.width
            spacing: 16

            ActionTile {
                Layout.fillWidth: true
                Layout.preferredHeight: 248
                title: "Record"
                detail: "Screen, window, or game"
                marker: "REC"
                accent: "#F05B4F"
                onClicked: appController.startRecording()
            }

            ActionTile {
                Layout.fillWidth: true
                Layout.preferredHeight: 248
                title: "Stream"
                detail: "Broadcast when you are ready"
                marker: "LIVE"
                accent: "#1F9D8B"
                onClicked: appController.startStreaming()
            }

            ActionTile {
                Layout.fillWidth: true
                Layout.preferredHeight: 248
                title: "Record + Stream"
                detail: "Keep a local copy while live"
                marker: "DUAL"
                accent: "#6E70D8"
                onClicked: appController.startRecordAndStream()
            }
        }

        Item {
            width: 1
            height: 34
        }

        Rectangle {
            width: parent.width
            height: 152
            radius: 8
            color: "#FFFFFF"
            border.color: "#E1E7E9"

            Row {
                anchors {
                    fill: parent
                    margins: 28
                }
                spacing: 28

                Rectangle {
                    width: 6
                    height: parent.height
                    radius: 3
                    color: "#DCE9E7"
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 7

                    Text {
                        text: "Workspace"
                        color: "#13202A"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: "No recording or stream is active."
                        color: "#73828A"
                        font.pixelSize: 14
                    }
                }
            }
        }
    }
}
