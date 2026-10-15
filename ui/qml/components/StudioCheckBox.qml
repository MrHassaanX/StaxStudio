import QtQuick
import QtQuick.Controls

CheckBox {
    id: root
    spacing: 8
    font.pixelSize: 12
    contentItem: Text {
        text: root.text
        color: root.enabled ? "#DDE9E9" : "#63777B"
        font: root.font
        leftPadding: root.indicator.width + root.spacing
        verticalAlignment: Text.AlignVCenter
    }
    indicator: Rectangle {
        implicitWidth: 17
        implicitHeight: 17
        radius: 3
        y: (root.height - height) / 2
        color: root.checked ? "#A7D56C" : "#17262B"
        border.color: root.activeFocus ? "#B7E65C" : "#607C79"
        Text { anchors.centerIn: parent; text: root.checked ? "✓" : ""; color: "#142019"; font.pixelSize: 13; font.weight: Font.Bold }
    }
}
