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
    property bool canMoveUp: true
    property bool canMoveDown: true
    objectName: "sourceRow_" + itemId
    signal selectedRequested()
    signal renameRequested()
    signal visibleRequested(bool value)
    signal lockedRequested(bool value)
    signal removeRequested()
    signal moveRequested(int direction)
    signal moveToRequested(int index)
    signal transformRequested(string action)
    signal propertiesRequested()
    height: 52
    radius: 5
    color: selected ? "#213B43" : (mouse.containsMouse ? "#1D2D33" : "transparent")
    border.color: selected ? "#397485" : "transparent"
    Rectangle { anchors.left: parent.left; anchors.leftMargin: 9; anchors.verticalCenter: parent.verticalCenter; width: 4; height: 25; radius: 2; color: root.type === "Image" ? "#D67B92" : (root.type === "Text" ? "#A493DE" : "#71A8B0") }
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
    StudioMenu {
        id: options
        x: root.width - width
        y: root.height
        StudioMenuItem { text: "Rename"; onTriggered: root.renameRequested() }
        StudioMenuItem { text: root.itemVisible ? "Hide source" : "Show source"; onTriggered: root.visibleRequested(!root.itemVisible) }
        StudioMenuItem { text: root.itemLocked ? "Unlock source" : "Lock source"; onTriggered: root.lockedRequested(!root.itemLocked) }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: "#31454B" } }
        StudioMenu {
            title: "Order"
            StudioMenuItem { text: "Move to top"; enabled: root.canMoveUp; onTriggered: root.moveToRequested(0) }
            StudioMenuItem { text: "Move up"; enabled: root.canMoveUp; onTriggered: root.moveRequested(-1) }
            StudioMenuItem { text: "Move down"; enabled: root.canMoveDown; onTriggered: root.moveRequested(1) }
            StudioMenuItem { text: "Move to bottom"; enabled: root.canMoveDown; onTriggered: root.moveToRequested(-1) }
        }
        StudioMenu {
            title: "Transform"
            StudioMenuItem { text: "Edit Transform"; onTriggered: root.transformRequested("edit") }
            StudioMenuItem { text: "Reset Transform"; onTriggered: root.transformRequested("reset") }
            MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: "#31454B" } }
            StudioMenuItem { text: "Fit to canvas"; onTriggered: root.transformRequested("fit") }
            StudioMenuItem { text: "Stretch to canvas"; onTriggered: root.transformRequested("stretch") }
            StudioMenuItem { text: "Center on canvas"; onTriggered: root.transformRequested("center") }
            StudioMenuItem { text: "Center horizontally"; onTriggered: root.transformRequested("centerHorizontal") }
            StudioMenuItem { text: "Center vertically"; onTriggered: root.transformRequested("centerVertical") }
            MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: "#31454B" } }
            StudioMenuItem { text: "Rotate 90 clockwise"; onTriggered: root.transformRequested("rotate90Clockwise") }
            StudioMenuItem { text: "Rotate 90 counterclockwise"; onTriggered: root.transformRequested("rotate90CounterClockwise") }
            StudioMenuItem { text: "Flip horizontal"; onTriggered: root.transformRequested("flipHorizontal") }
            StudioMenuItem { text: "Flip vertical"; onTriggered: root.transformRequested("flipVertical") }
        }
        StudioMenuItem { text: "Properties"; onTriggered: root.propertiesRequested() }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: "#31454B" } }
        StudioMenuItem { text: "Remove from scene"; destructive: true; onTriggered: root.removeRequested() }
    }
}
