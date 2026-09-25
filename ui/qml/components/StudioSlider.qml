import QtQuick
import QtQuick.Controls

Slider {
    id: root
    property real referenceValue: NaN
    implicitHeight: 20
    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: 5
        radius: 3
        color: "#2B3A40"
        Rectangle { width: root.visualPosition * parent.width; height: parent.height; radius: 3; color: root.enabled ? "#8CAE80" : "#526066" }
        Rectangle { visible: isFinite(root.referenceValue); x: (root.referenceValue-root.from)/(root.to-root.from)*parent.width-1; y:-3; width:2; height:11; color:"#D7E5E6" }
    }
    handle: Rectangle { x: root.leftPadding + root.visualPosition * (root.availableWidth - width); y: root.topPadding + root.availableHeight / 2 - height / 2; width: 14; height: 14; radius: 7; color: root.pressed ? "#D8E9CE" : "#B7E65C"; border.color: "#0E1517" }
}
