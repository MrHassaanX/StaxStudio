import QtQuick

Item {
    id: root
    property bool muted: false
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
                height: parent.height
                radius: 1
                color: root.muted ? "#3F3535" : (index === 3 ? "#56676C" : "#34474C")
            }
        }
        Text { text: "IN"; color: root.muted ? "#796966" : "#71858B"; font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }
    }
}
