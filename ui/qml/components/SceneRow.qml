import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property string sceneId
    required property string name
    required property bool active
    signal selected()
    signal renameRequested()
    signal deleteRequested()
    signal moveRequested(int direction)
    implicitHeight: 42
    radius: 5
    color: active ? "#304439" : (mouse.containsMouse ? "#1D2B31" : "transparent")
    border.color: active ? "#6B8B61" : "transparent"
    Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.right: menuButton.left; anchors.rightMargin: 4; anchors.verticalCenter: parent.verticalCenter; text: root.name; elide: Text.ElideRight; color: root.active ? "#F4F8F0" : "#C8D3D5"; font.pixelSize: 13; font.weight: root.active ? Font.DemiBold : Font.Normal }
    IconButton { id: menuButton; anchors.right: parent.right; anchors.rightMargin: 4; anchors.verticalCenter: parent.verticalCenter; iconName: "more"; tooltip: "Scene options"; onClicked: options.open() }
    MouseArea { id: mouse; anchors.left: parent.left; anchors.right: menuButton.left; anchors.top: parent.top; anchors.bottom: parent.bottom; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.selected(); onDoubleClicked: root.renameRequested() }
    Menu {
        id: options
        MenuItem { text: "Rename"; onTriggered: root.renameRequested() }
        MenuItem { text: "Move up"; onTriggered: root.moveRequested(-1) }
        MenuItem { text: "Move down"; onTriggered: root.moveRequested(1) }
        MenuSeparator { }
        MenuItem { text: "Delete scene"; onTriggered: root.deleteRequested() }
    }
}
