import QtQuick
import QtQuick.Controls

Item {
    id: root
    required property var manager
    property var minimumLayout: { const state = manager.state; return manager.arrange(0, 0) }
    property var arrangement: { const state = manager.state; return manager.arrange(width, height) }
    property string draggingPanel: ""
    property string targetPanel: ""
    property string targetEdge: ""
    property rect dropRect: Qt.rect(0,0,0,0)
    function panelRect(id) { return arrangement.panels[id] || {x:0,y:0,width:0,height:0} }
    function dragAt(panel, point) {
        draggingPanel = panel; targetPanel = ""
        for (let id in arrangement.panels) {
            if (id === panel) continue
            const r = panelRect(id)
            if (point.x < r.x || point.x > r.x+r.width || point.y < r.y || point.y > r.y+r.height) continue
            const x = (point.x-r.x)/r.width, y = (point.y-r.y)/r.height
            const distance = Math.min(x,1-x,y,1-y)
            targetEdge = distance === x ? "left" : distance === 1-x ? "right" : distance === y ? "top" : "bottom"
            targetPanel = id
            dropRect = Qt.rect(r.x,r.y,r.width,r.height)
            if (targetEdge === "left") dropRect.width /= 2
            if (targetEdge === "right") { dropRect.x += r.width/2; dropRect.width /= 2 }
            if (targetEdge === "top") dropRect.height /= 2
            if (targetEdge === "bottom") { dropRect.y += r.height/2; dropRect.height /= 2 }
            break
        }
    }
    function finishDrag() {
        if (targetPanel.length) manager.movePanel(draggingPanel,targetPanel,targetEdge)
        draggingPanel = ""; targetPanel = ""
    }
    Repeater {
        model: root.arrangement.handles.length
        delegate: Rectangle {
            required property int index
            property var modelData: root.arrangement.handles[index]
            x:modelData.x; y:modelData.y; width:modelData.width; height:modelData.height
            z:20; color: drag.containsMouse || drag.pressed ? "#6E8D93" : "#26383E"
            objectName: "dockSplitter_" + modelData.id
            MouseArea {
                id:drag; anchors.fill:parent; hoverEnabled:true; enabled:!root.manager.locked
                cursorShape:parent.modelData.horizontal ? Qt.SplitHCursor : Qt.SplitVCursor
                property var splitInfo
                onPressed: splitInfo = parent.modelData
                onPositionChanged: mouse => {
                    if (!pressed) return
                    const p=mapToItem(root,mouse.x,mouse.y)
                    root.manager.resizeSplit(splitInfo.id,((splitInfo.horizontal ? p.x : p.y)-splitInfo.origin)/splitInfo.extent)
                }
                onReleased: root.manager.save()
            }
        }
    }
    Rectangle {
        z:100; visible:root.draggingPanel.length>0 && root.targetPanel.length>0
        x:root.dropRect.x; y:root.dropRect.y; width:root.dropRect.width; height:root.dropRect.height
        color:"#405E7780"; border.color:"#C3EB65"; border.width:2
        Text { anchors.centerIn:parent; text: "Dock " + root.targetEdge; color:"#FFFFFF"; font.pixelSize:13 }
    }
}
