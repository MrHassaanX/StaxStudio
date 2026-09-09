import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    anchors.fill: parent

    Column {
        anchors {
            fill: parent
            margins: 52
        }
        spacing: 0

        Text {
            text: "Stream"
            color: "#13202A"
            font.pixelSize: 30
            font.weight: Font.DemiBold
        }

        Item {
            width: 1
            height: 10
        }

        Text {
            text: "Prepare a live destination and session profile."
            color: "#60717A"
            font.pixelSize: 15
        }

        Item {
            width: 1
            height: 30
        }

        RowLayout {
            width: parent.width
            spacing: 18

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 292
                radius: 8
                color: "#FFFFFF"
                border.color: "#E1E7E9"

                Column {
                    anchors {
                        fill: parent
                        margins: 28
                    }
                    spacing: 12

                    Text {
                        text: "Destination"
                        color: "#13202A"
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: "No live destination is connected."
                        color: "#73828A"
                        font.pixelSize: 14
                    }

                    ComboBox {
                        width: parent.width
                        model: ["Custom RTMP", "YouTube", "Twitch"]
                    }

                    Item {
                        width: 1
                        height: 8
                    }

                    Text {
                        text: "Output"
                        color: "#13202A"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }

                    Row {
                        spacing: 10

                        Rectangle {
                            width: 102
                            height: 34
                            radius: 5
                            color: "#EEF5F4"

                            Text {
                                anchors.centerIn: parent
                                text: "1080p"
                                color: "#246A60"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                        }

                        Rectangle {
                            width: 84
                            height: 34
                            radius: 5
                            color: "#F1F3F5"

                            Text {
                                anchors.centerIn: parent
                                text: "6 Mbps"
                                color: "#4D5B63"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 254
                Layout.preferredHeight: 292
                radius: 8
                color: "#163B3E"

                Column {
                    anchors {
                        fill: parent
                        margins: 28
                    }
                    spacing: 12

                    Text {
                        text: "Live status"
                        color: "#FFFFFF"
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: "#7C9C9B"
                    }

                    Text {
                        text: "Offline"
                        color: "#C1D3D1"
                        font.pixelSize: 14
                    }

                    Item {
                        width: 1
                        height: 58
                    }

                    Button {
                        width: parent.width
                        text: "Go live"
                        enabled: false
                    }
                }
            }
        }
    }
}
