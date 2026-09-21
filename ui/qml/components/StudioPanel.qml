import QtQuick

Rectangle {
    id: root
    default property alias content: contentHost.data
    property int contentMargin: 14

    radius: 6
    color: "#162126"
    border.color: "#2B3C43"
    border.width: 1
    clip: true

    Item {
        id: contentHost
        anchors.fill: parent
        anchors.margins: root.contentMargin
    }
}
