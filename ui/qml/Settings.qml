import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "components"

Item {
    id: root
    objectName: "settingsPage"
    anchors.fill: parent
    property string section: "General"
    property string exportMessage: ""

    FileDialog {
        id: exportDialog
        title: "Export recording diagnostics"
        fileMode: FileDialog.SaveFile
        nameFilters: ["JSON Lines (*.jsonl)"]
        onAccepted: {
            root.exportMessage = studioController.exportRecordingDiagnostics(selectedFile.toString())
                               ? "Diagnostics exported." : studioController.statusMessage
        }
    }

    Rectangle { anchors.fill: parent; color: "#0F171A" }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16
        Text { text: "Settings"; color: "#F5F8F8"; font.pixelSize: 22; font.weight: Font.DemiBold }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ColumnLayout {
                Layout.preferredWidth: 130
                Layout.alignment: Qt.AlignTop
                Repeater {
                    model: ["General", "Stream", "Output", "Audio", "Video", "Hotkeys", "Appearance", "Advanced"]
                    delegate: StudioButton {
                        required property string modelData
                        objectName: "settingsCategory_" + modelData
                        text: modelData
                        checked: root.section === modelData
                        onClicked: root.section = modelData
                    }
                }
            }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 5
            color: "#162126"
            border.color: "#2B3C43"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 12
                Text { text: root.section; color: "#EAF0F1"; font.pixelSize: 16; font.weight: Font.DemiBold }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#2F4148" }
                ColumnLayout {
                    visible: root.section === "General"
                    spacing: 8
                    Text { text: "Smart mode"; color: "#DDE9E9"; font.pixelSize: 13; font.weight: Font.DemiBold }
                    Text { text: "Hardware recommendations are not configured."; color: "#8FA3A7"; font.pixelSize: 12 }
                }
                ColumnLayout {
                    visible: root.section === "Advanced"
                    spacing: 10
                    Text { text: "Diagnostics"; color: "#DDE9E9"; font.pixelSize: 13; font.weight: Font.DemiBold }
                    StudioCheckBox {
                        objectName: "detailedDiagnosticsToggle"
                        text: "Enable detailed recording diagnostics"
                        checked: studioController.detailedRecordingDiagnostics
                        onToggled: studioController.setDetailedRecordingDiagnostics(checked)
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "When enabled, recording logs are saved in the application logs folder, not beside your videos. The setting applies to new recordings."
                        color: "#8FA3A7"; font.pixelSize: 12; wrapMode: Text.WordWrap
                    }
                    StudioButton { text: "Export Diagnostics"; onClicked: exportDialog.open() }
                    Text { visible: root.exportMessage.length > 0; text: root.exportMessage; color: "#A7D56C"; font.pixelSize: 12 }
                }
                Stream { Layout.fillWidth: true; Layout.fillHeight: true; visible: root.section === "Stream" }
                Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; visible: ["General", "Stream", "Advanced"].indexOf(root.section) < 0; text: root.section + " preferences are unavailable in this milestone. Use existing Studio controls for output and canvas settings."; color: "#8FA3A7" }
                Item { Layout.fillHeight: true; visible: root.section !== "Stream" }
            }
        }
        }
    }
}

