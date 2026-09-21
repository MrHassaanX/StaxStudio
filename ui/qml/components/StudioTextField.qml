import QtQuick
import QtQuick.Controls

TextField {
    id: root
    implicitHeight: 36
    color: "#E6EFF0"
    placeholderTextColor: "#71848A"
    leftPadding: 10
    rightPadding: 10
    selectionColor: "#557565"
    selectedTextColor: "#F4F8F7"
    background: Rectangle { radius: 4; color: root.enabled ? "#10191D" : "#182227"; border.color: root.activeFocus ? "#739C84" : "#3A5058" }
}
