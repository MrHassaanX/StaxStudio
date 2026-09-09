import QtQuick

Item {
    id: root

    required property string title
    required property string detail
    required property string marker
    required property color accent
    property bool hovering: false

    signal clicked()

    implicitWidth: 240
    implicitHeight: 248

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: root.hovering ? "#FBFCFC" : "#FFFFFF"
        border.color: root.hovering ? root.accent : "#E1E7E9"
        border.width: root.hovering ? 2 : 1

        Rectangle {
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }
            height: 6
            radius: 3
            color: root.accent
        }

        Column {
            anchors {
                fill: parent
                margins: 24
            }
            spacing: 0

            Text {
                text: root.marker
                color: root.accent
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            Item {
                width: 1
                height: 48
            }

            Text {
                width: parent.width
                text: root.title
                color: "#13202A"
                font.pixelSize: 22
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            Item {
                width: 1
                height: 9
            }

            Text {
                width: parent.width
                text: root.detail
                color: "#60717A"
                font.pixelSize: 14
                wrapMode: Text.Wrap
            }

            Item {
                width: 1
                height: 1
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onEntered: root.hovering = true
            onExited: root.hovering = false
            onClicked: root.clicked()
        }
    }
}
