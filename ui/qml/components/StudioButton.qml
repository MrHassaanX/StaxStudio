import QtQuick
import QtQuick.Controls

Button {
    id: root
    implicitHeight: 38
    font.pixelSize: 13
    font.weight: Font.DemiBold
    contentItem: Text { text: root.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: root.enabled ? "#EAF0F1" : "#607076"; font: root.font }
    background: Rectangle { radius: 4; color: root.enabled ? (root.checked ? "#3C5D50" : (root.down ? "#416255" : (root.hovered ? "#385548" : "#30473D"))) : "#20292C"; border.color: root.enabled ? (root.checked ? "#8AAC7D" : "#617C6B") : "#2B3539" }
}
