import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property string itemId
    required property string sourceId
    required property string name
    required property string type
    required property bool itemVisible
    required property bool itemLocked
    required property bool selected
    signal selectedRequested()
    signal renameRequested()
    signal visibleRequested(bool value)
    signal lockedRequested(bool value)
    signal removeRequested()
    signal moveRequested(int direction)
    implicitHeight: 52
    radius: 5
    color: selected ? "#213B43" : (mouse.containsMouse ? "#1D2D33" : "transparent")
    border.color: selected ? "#397485" : "transparent"
    Rectangle { anchors.left: parent.left; anchors.leftMargin: 9; anchors.verticalCenter: parent.verticalCenter; width: 4; height: 25; radius: 2; color: type === "Image" ? "#D67B92" : (type === "Text" ? "#A493DE" : "#71A8B0") }
    Column { anchors.left: parent.left; anchors.leftMargin: 20; anchors.right: controls.left; anchors.rightMargin: 5; anchors.verticalCenter: parent.verticalCenter; spacing: 2
        Text { width: parent.width; text: root.name; elide: Text.ElideRight; color: root.itemVisible ? "#E8EFF0" : "#809096"; font.pixelSize: 12; font.weight: Font.DemiBold }
        Text { width: parent.width; text: root.type + " placeholder"; elide: Text.ElideRight; color: "#7E9197"; font.pixelSize: 10 }
    }
    Row { id: controls; anchors.right: parent.right; anchors.rightMargin: 3; anchors.verticalCenter: parent.verticalCenter; spacing: 0
        IconButton { iconName: root.itemVisible ? "eye" : "eyeOff"; tooltip: root.itemVisible ? "Hide source" : "Show source"; onClicked: root.visibleRequested(!root.itemVisible) }
        IconButton { iconName: root.itemLocked ? "lock" : "unlock"; tooltip: root.itemLocked ? "Unlock source" : "Lock source"; onClicked: root.lockedRequested(!root.itemLocked) }
        IconButton { iconName: "more"; tooltip: "Source options"; onClicked: options.open() }
    }
    MouseArea { id: mouse; anchors.left: parent.left; anchors.right: controls.left; anchors.top: parent.top; anchors.bottom: parent.bottom; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.selectedRequested(); onDoubleClicked: root.renameRequested() }
    Menu {
        id: options
        MenuItem { text: "Rename"; onTriggered: root.renameRequested() }
        MenuItem { text: "Move layer forward"; onTriggered: root.moveRequested(1) }
        MenuItem { text: "Move layer backward"; onTriggered: root.moveRequested(-1) }
        MenuSeparator { }
        MenuItem { text: "Remove from scene"; onTriggered: root.removeRequested() }
    }
}
