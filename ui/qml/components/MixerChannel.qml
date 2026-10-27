import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property string channelId
    required property string name
    required property double volume
    required property bool muted
    required property double levelDb
    required property string sourceState
    required property int channelCount
    objectName: "mixerRow_" + channelId
    implicitWidth: 106
    implicitHeight: 125
    signal volumeChangedByUser(double value)
    signal muteRequested(bool muted)
    signal renameRequested()
    signal removeRequested()
    signal resetVolumeRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 3
        Text {
            Layout.fillWidth: true
            text: root.name
            color: root.muted ? "#809095" : "#E4ECEE"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            ToolTip.visible: nameHover.containsMouse
            ToolTip.text: root.name
            MouseArea { id: nameHover; anchors.fill: parent; hoverEnabled: true }
        }
        Text {
            Layout.fillWidth: true
            text: root.sourceState + " · " + (root.channelCount === 1 ? "Mono" : root.channelCount === 2 ? "Stereo" : root.channelCount > 2 ? root.channelCount + " ch" : "No signal")
            color: root.sourceState === "Active" ? "#9DB3AF" : "#D5A77B"
            font.pixelSize: 9
            elide: Text.ElideRight
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 5
            Item { Layout.fillWidth: true }
            AudioMeter {
                Layout.preferredWidth: 42
                Layout.fillHeight: true
                muted: root.muted
                levelDb: root.levelDb
            }
            StudioVerticalSlider {
                id: fader
                objectName: "mixerFader_" + root.channelId
                Layout.preferredWidth: 24
                Layout.fillHeight: true
                from: -60
                to: 0
                stepSize: 0.5
                value: root.volume > 0 ? Math.max(-60, 20 * Math.log(root.volume) / Math.LN10) : -60
                onMoved: root.volumeChangedByUser(value <= -60 ? 0 : Math.pow(10, value / 20))
                ToolTip.visible: pressed
                ToolTip.text: root.volume <= 0 ? "-inf dB" : (20 * Math.log(root.volume) / Math.LN10).toFixed(1) + " dB"
            }
            Item { Layout.fillWidth: true }
        }
        Text {
            Layout.fillWidth: true
            text: root.muted ? "MUTED" : root.volume <= 0 ? "-inf dB" : (20 * Math.log(root.volume) / Math.LN10).toFixed(1) + " dB"
            color: root.muted ? "#A88D85" : "#B2C7C8"
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 1
            IconButton { iconName: root.muted ? "mute" : "volume"; tooltip: root.muted ? "Unmute channel" : "Mute channel"; onClicked: root.muteRequested(!root.muted) }
            IconButton { iconName: "more"; tooltip: "Audio options"; onClicked: options.open() }
        }
    }
    StudioMenu {
        id: options
        StudioMenuItem { text: root.muted ? "Unmute" : "Mute"; onTriggered: root.muteRequested(!root.muted) }
        StudioMenuItem { text: "Reset to 0 dB"; onTriggered: root.resetVolumeRequested() }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: "#31454B" } }
        StudioMenuItem { text: "Rename source"; onTriggered: root.renameRequested() }
        StudioMenuItem { text: "Remove from scene"; destructive: true; onTriggered: root.removeRequested() }
    }
}
