import QtQuick
import QtQuick.Controls

MenuItem {
    id: root
    property bool destructive: false
    objectName: text
    implicitWidth: 172
    implicitHeight: 34
    contentItem: Text { text: root.text; color: root.destructive ? "#F29A92" : (root.enabled ? "#DCE7E8" : "#65777C"); font.pixelSize: 12; verticalAlignment: Text.AlignVCenter; leftPadding: 10 }
    background: Rectangle { radius: 3; color: root.highlighted ? (root.destructive ? "#4A2B2B" : "#29413F") : "transparent" }
}
