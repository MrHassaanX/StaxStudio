import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property string sceneId
    required property string name
    required property bool active
    property bool canDelete: true
    property bool canMoveUp: true
    property bool canMoveDown: true
    objectName: "sceneRow_" + sceneId
    signal selected()
    signal renameRequested()
    signal deleteRequested()
    signal moveRequested(int direction)
    height: 42
    radius: 5
    color: active ? "#304439" : (mouse.containsMouse ? "#1D2B31" : "transparent")
    border.color: active ? "#6B8B61" : "transparent"
    Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.right: menuButton.left; anchors.rightMargin: 4; anchors.verticalCenter: parent.verticalCenter; text: root.name; elide: Text.ElideRight; color: root.active ? "#F4F8F0" : "#C8D3D5"; font.pixelSize: 13; font.weight: root.active ? Font.DemiBold : Font.Normal }
    IconButton { id: menuButton; anchors.right: parent.right; anchors.rightMargin: 4; anchors.verticalCenter: parent.verticalCenter; iconName: "more"; tooltip: "Scene options"; onClicked: options.open() }
    MouseArea { id: mouse; anchors.left: parent.left; anchors.right: menuButton.left; anchors.top: parent.top; anchors.bottom: parent.bottom; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.selected(); onDoubleClicked: root.renameRequested() }
    StudioMenu {
        id: options
        x: root.width - width
        y: root.height
        StudioMenuItem { text: "Rename"; onTriggered: root.renameRequested() }
        StudioMenuItem { text: "Move up"; enabled: root.canMoveUp; onTriggered: root.moveRequested(-1) }
        StudioMenuItem { text: "Move down"; enabled: root.canMoveDown; onTriggered: root.moveRequested(1) }
        MenuSeparator { }
        StudioMenuItem { text: "Delete scene"; enabled: root.canDelete; destructive: true; onTriggered: root.deleteRequested() }
    }
}
