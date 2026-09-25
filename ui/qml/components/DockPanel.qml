import QtQuick

Rectangle {
    id:root
    required property string dockId
    required property string dockTitle
    required property var workspace
    default property alias content: contentHost.data
    property var placement: workspace.panelRect(dockId)
    x:placement.x; y:placement.y; width:placement.width; height:placement.height
    visible: width>0 && height>0
    color:"#162126"; border.color:"#2B3C43"; radius:5; clip:true
    Rectangle {
        x:1; y:1; width:parent.width-2; height:27; color:"#1E2D32"
        Text { anchors.fill:parent; anchors.leftMargin:10; anchors.rightMargin:10; verticalAlignment:Text.AlignVCenter; text:root.dockTitle; color:"#D8E4E6"; font.pixelSize:12; font.weight:Font.DemiBold; elide:Text.ElideRight }
        MouseArea {
            objectName: "dockHeader_" + root.dockId
            anchors.fill:parent; enabled:!root.workspace.manager.locked
            cursorShape:pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            property point start
            onPressed: mouse => start=Qt.point(mouse.x,mouse.y)
            onPositionChanged: mouse => {
                if(pressed && (Math.abs(mouse.x-start.x)+Math.abs(mouse.y-start.y)>6))
                    root.workspace.dragAt(root.dockId,mapToItem(root.workspace,mouse.x,mouse.y))
            }
            onReleased: root.workspace.finishDrag()
            onCanceled: {root.workspace.draggingPanel="";root.workspace.targetPanel=""}
        }
    }
    Item { id:contentHost; x:10; y:35; width:Math.max(0,parent.width-20); height:Math.max(0,parent.height-45); clip:true }
}
