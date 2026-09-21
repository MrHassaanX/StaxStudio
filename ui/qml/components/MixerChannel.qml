import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property string channelId
    required property string name
    required property double volume
    required property bool muted
    signal volumeChangedByUser(double value)
    signal muteRequested(bool muted)
    Layout.fillWidth: true
    height: 64
    spacing: 6
    RowLayout { Layout.fillWidth: true
        Text { Layout.fillWidth: true; text: root.name; color: root.muted ? "#809095" : "#E4ECEE"; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }
        IconButton { iconName: root.muted ? "mute" : "volume"; tooltip: root.muted ? "Unmute channel" : "Mute channel"; onClicked: root.muteRequested(!root.muted) }
        IconButton { iconName: "more"; tooltip: "Audio options"; onClicked: options.open() }
    }
    RowLayout { Layout.fillWidth: true; spacing: 8
        Rectangle { Layout.preferredWidth: 42; Layout.preferredHeight: 6; radius: 3; color: "#344348"; Rectangle { width: parent.width * .28; height: parent.height; radius: 3; color: root.muted ? "#536166" : "#6E9081" } }
        StudioSlider { Layout.fillWidth: true; from: 0; to: 1; value: root.volume; onMoved: root.volumeChangedByUser(value) }
        Text { text: Math.round(root.volume * 100) + "%"; color: "#87979C"; font.pixelSize: 10; width: 30; horizontalAlignment: Text.AlignRight }
    }
    StudioMenu { id: options; StudioMenuItem { text: "Audio settings"; enabled: false } }
}
