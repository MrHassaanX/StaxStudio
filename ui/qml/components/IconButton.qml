import QtQuick
import QtQuick.Controls

ToolButton {
    id: root
    property string iconName: "more"
    property string tooltip: ""
    implicitWidth: 30
    implicitHeight: 30
    contentItem: StudioIcon { anchors.centerIn: parent; name: root.iconName; color: root.enabled ? (root.hovered ? "#EDF4F5" : "#B6C7CB") : "#5F7076" }
    background: Rectangle { radius: 4; color: root.down ? "#2D4248" : (root.hovered ? "#24363D" : "transparent") }
    ToolTip.visible: root.hovered && root.tooltip.length > 0
    ToolTip.text: root.tooltip
}
