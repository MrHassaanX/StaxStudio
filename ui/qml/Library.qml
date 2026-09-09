import QtQuick

Item {
    anchors.fill: parent

    Column {
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: 52
        }
        spacing: 0

        Text {
            text: "Library"
            color: "#13202A"
            font.pixelSize: 30
            font.weight: Font.DemiBold
        }

        Item {
            width: 1
            height: 10
        }

        Text {
            text: "Your local recordings and saved sessions."
            color: "#60717A"
            font.pixelSize: 15
        }

        Item {
            width: 1
            height: 38
        }

        Rectangle {
            width: parent.width
            height: 270
            radius: 8
            color: "#FFFFFF"
            border.color: "#E1E7E9"

            Column {
                anchors.centerIn: parent
                spacing: 12

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 52
                    height: 42
                    radius: 6
                    color: "#EAF1F0"

                    Rectangle {
                        anchors.centerIn: parent
                        width: 24
                        height: 4
                        radius: 2
                        color: "#87A39E"
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "No recordings yet"
                    color: "#13202A"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Completed sessions will appear here."
                    color: "#73828A"
                    font.pixelSize: 14
                }
            }
        }
    }
}
