import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property string channelId
    required property string name
    required property double volume
    required property bool muted
    required property double levelDb
    objectName: "mixerRow_" + channelId
    signal volumeChangedByUser(double value)
    signal muteRequested(bool muted)
    signal renameRequested()
    signal removeRequested()
    signal resetVolumeRequested()
    Layout.fillWidth: true
    height: 58
    spacing: 4
    RowLayout { Layout.fillWidth: true
        Text { Layout.fillWidth: true; text: root.name; color: root.muted ? "#809095" : "#E4ECEE"; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }
        Text { text: root.muted ? "MUTED" : (20 * Math.log(Math.max(0.001, root.volume)) / Math.LN10).toFixed(1) + " dB"; color: root.muted ? "#8B7772" : "#9CB4B9"; font.pixelSize: 9; Layout.preferredWidth: 40; horizontalAlignment: Text.AlignRight }
        IconButton { iconName: root.muted ? "mute" : "volume"; tooltip: root.muted ? "Unmute channel" : "Mute channel"; onClicked: root.muteRequested(!root.muted) }
        IconButton { iconName: "more"; tooltip: "Audio options"; onClicked: options.open() }
    }
    RowLayout { Layout.fillWidth: true; spacing: 7
        AudioMeter { Layout.preferredWidth: 27; Layout.preferredHeight: 21; muted: root.muted; levelDb: root.levelDb }
        StudioSlider { Layout.fillWidth: true; from: 0; to: 1; value: root.volume; onMoved: root.volumeChangedByUser(value) }
        Text { text: Math.round(root.volume * 100) + "%"; color: "#87979C"; font.pixelSize: 10; Layout.preferredWidth: 30; horizontalAlignment: Text.AlignRight }
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
