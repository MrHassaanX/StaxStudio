import QtQuick
import QtQuick.Controls

Dialog {
    id: root
    modal: true
    focus: true
    padding: 16
    background: Rectangle { color: "#18262B"; radius: 7; border.color: "#49616A" }
    header: Label { text: root.title; color: "#F2F7F7"; font.pixelSize: 16; font.weight: Font.DemiBold; leftPadding: 16; topPadding: 16 }
}
