import QtQuick
import QtQuick.Controls

Slider {
    id: root
    orientation: Qt.Vertical
    implicitWidth: 24
    implicitHeight: 88
    background: Rectangle {
        x: root.leftPadding + (root.availableWidth - width) / 2
        y: root.topPadding
        width: 5
        height: root.availableHeight
        radius: 2
        color: "#2B3A40"
        Rectangle {
            y: root.visualPosition * parent.height
            width: parent.width
            height: parent.height - y
            radius: 2
            color: root.enabled ? "#8CAE80" : "#526066"
        }
    }
    handle: Rectangle {
        x: root.leftPadding + (root.availableWidth - width) / 2
        y: root.topPadding + root.visualPosition * (root.availableHeight - height)
        width: 17
        height: 10
        radius: 3
        color: root.pressed ? "#E2EED4" : "#B7E65C"
        border.color: "#0E1517"
    }
}
