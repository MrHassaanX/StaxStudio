import QtQuick
import QtQuick.Controls

Menu {
    id: root
    padding: 4
    background: Rectangle {
        implicitWidth: 188
        implicitHeight: 40
        color: "#18262B"; radius: 5; border.color: "#40565D"
    }
    delegate: StudioMenuItem { }
}
