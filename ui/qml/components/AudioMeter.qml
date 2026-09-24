import QtQuick

Item {
    id: root
    property bool muted: false
    property double levelDb: -90
    implicitWidth: 27
    implicitHeight: 21

    Row {
        anchors.fill: parent
        spacing: 2
        Repeater {
            model: 4
            delegate: Rectangle {
                required property int index
                width: 4
                height: Math.max(3, parent.height * Math.max(0, Math.min(1, (root.levelDb + 60) / 60)))
                anchors.bottom: parent.bottom
                radius: 1
                color: root.muted ? "#3F3535" : (index === 3 ? "#56676C" : "#34474C")
            }
        }
        Text { text: "IN"; color: root.muted ? "#796966" : "#71858B"; font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }
    }
}
