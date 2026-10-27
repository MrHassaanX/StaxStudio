import QtQuick

Item {
    id: root
    property bool muted: false
    property double levelDb: -90
    implicitWidth: 42
    implicitHeight: 72
    readonly property real fillFraction: Math.max(0, Math.min(1, (levelDb + 90) / 90))

    Column {
        x: 0
        y: 0
        width: 25
        height: parent.height
        Repeater {
            model: ["0", "-24", "-48", "-90"]
            delegate: Text {
                required property string modelData
                width: 25
                height: root.height / 4
                text: modelData
                color: "#81969B"
                font.pixelSize: 8
            }
        }
    }
    Rectangle {
        x: 27
        y: 1
        width: 11
        height: Math.max(0, parent.height - 2)
        radius: 2
        color: "#25343A"
        Rectangle {
            width: parent.width
            height: root.fillFraction * parent.height
            y: parent.height - height
            radius: 2
            color: root.muted ? "#566467" : root.levelDb > -6 ? "#DB7469" : root.levelDb > -18 ? "#D6AA63" : "#8EBC7A"
        }
    }
}
