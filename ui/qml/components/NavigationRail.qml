import QtQuick

Item {
    id: root
    clip: true

    property string currentPage: appController.activePage

    Rectangle {
        anchors.fill: parent
        color: "#13202A"

        Column {
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                margins: 18
            }
            spacing: 0

            Row {
                spacing: 11

                Rectangle {
                    width: 34
                    height: 34
                    radius: 7
                    color: "#E9FB81"

                    Rectangle {
                        x: 9
                        y: 9
                        width: 16
                        height: 5
                        radius: 2
                        color: "#13202A"
                    }

                    Rectangle {
                        x: 9
                        y: 19
                        width: 10
                        height: 5
                        radius: 2
                        color: "#13202A"
                    }
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Text {
                        text: "StaxStudio"
                        color: "#FFFFFF"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: "FOUNDATION"
                        color: "#8EA4A8"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                }
            }

            Item { width: 1; height: 28 }

            Repeater {
                model: [
                    { "page": "home", "label": "Home", "number": "01" },
                    { "page": "studio", "label": "Studio", "number": "02" },
                    { "page": "record", "label": "Quick Record", "number": "03" },
                    { "page": "stream", "label": "Stream Setup", "number": "04" },
                    { "page": "library", "label": "Library", "number": "05" },
                    { "page": "settings", "label": "Settings", "number": "06" }
                ]

                delegate: Rectangle {
                    id: navigationItem

                    required property var modelData
                    property bool selected: root.currentPage === modelData.page
                    property bool hovering: false

                    width: parent.width
                    height: 44
                    radius: 6
                    color: selected ? "#25434A" : (hovering ? "#1C3037" : "transparent")

                    Text {
                        anchors {
                            left: parent.left
                        leftMargin: 12
                            verticalCenter: parent.verticalCenter
                        }
                        text: navigationItem.modelData.number
                        color: navigationItem.selected ? "#E9FB81" : "#779096"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    Text {
                        anchors {
                            left: parent.left
                        leftMargin: 43
                            right: parent.right
                            rightMargin: 10
                            verticalCenter: parent.verticalCenter
                        }
                        text: navigationItem.modelData.label
                        elide: Text.ElideRight
                        color: navigationItem.selected ? "#FFFFFF" : "#C7D4D6"
                        font.pixelSize: 14
                        font.weight: navigationItem.selected ? Font.DemiBold : Font.Normal
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: navigationItem.hovering = true
                        onExited: navigationItem.hovering = false
                        onClicked: appController.setActivePage(navigationItem.modelData.page)
                    }
                }
            }
        }

        Rectangle {
            id: smartModePanel
            objectName: "smartModePanel"
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                margins: 18
            }
            height: smartModeContent.implicitHeight + 24
            radius: 8
            color: "#1B3036"
            border.color: "#294850"

            Column {
                id: smartModeContent
                anchors {
                    fill: parent
                margins: 12
                }
                spacing: 5

                Text {
                    width: parent.width
                    text: "Smart mode"
                    color: "#F0F5F4"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                Text {
                    objectName: "smartModeDescription"
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: "Recommendations arrive with hardware setup"
                    color: "#8FA6A9"
                    font.pixelSize: 12
                }
            }
        }
    }
}
