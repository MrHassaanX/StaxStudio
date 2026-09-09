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
            text: "Settings"
            color: "#13202A"
            font.pixelSize: 30
            font.weight: Font.DemiBold
        }

        Item {
            width: 1
            height: 10
        }

        Text {
            text: "Keep common preferences in one place."
            color: "#60717A"
            font.pixelSize: 15
        }

        Item {
            width: 1
            height: 30
        }

        Rectangle {
            width: parent.width
            height: 238
            radius: 8
            color: "#FFFFFF"
            border.color: "#E1E7E9"

            Column {
                anchors {
                    fill: parent
                    margins: 28
                }
                spacing: 0

                RowLayout {
                    width: parent.width
                    height: 58

                    Column {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: "Smart mode"
                            color: "#13202A"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: "Hardware recommendations are unavailable."
                            color: "#73828A"
                            font.pixelSize: 14
                        }
                    }

                    Switch {
                        checked: false
                        enabled: false
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: "#E8ECEE"
                }

                RowLayout {
                    width: parent.width
                    height: 74

                    Column {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: "Recording location"
                            color: "#13202A"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: "No location selected."
                            color: "#73828A"
                            font.pixelSize: 14
                        }
                    }

                    Button {
                        text: "Choose"
                        enabled: false
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: "#E8ECEE"
                }

                RowLayout {
                    width: parent.width
                    height: 58

                    Column {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: "Encoder"
                            color: "#13202A"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: "No encoder selected."
                            color: "#73828A"
                            font.pixelSize: 14
                        }
                    }

                    Text {
                        text: "Automatic"
                        color: "#60717A"
                        font.pixelSize: 14
                    }
                }
            }
        }
    }
}
