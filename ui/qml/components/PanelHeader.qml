import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    required property string title
    property string subtitle: ""
    property string actionText: ""
    signal actionClicked()

    Layout.fillWidth: true
    spacing: 8

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        Text { text: root.title; color: "#F4F7F8"; font.pixelSize: 14; font.weight: Font.DemiBold }
        Text { visible: root.subtitle.length > 0; text: root.subtitle; color: "#839098"; font.pixelSize: 11 }
    }

    ToolButton {
        visible: root.actionText.length > 0
        text: root.actionText
        font.pixelSize: 14
        onClicked: root.actionClicked()
        ToolTip.visible: hovered
        ToolTip.text: root.actionText
    }
}
