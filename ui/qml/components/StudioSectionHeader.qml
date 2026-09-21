import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root
    required property string title
    property string subtitle: ""
    default property alias actions: actionHost.data
    Layout.fillWidth: true
    spacing: 8
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2
        Text { text: root.title; color: "#F2F6F7"; font.pixelSize: 14; font.weight: Font.DemiBold }
        Text { visible: root.subtitle.length > 0; text: root.subtitle; color: "#809299"; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
    }
    RowLayout { id: actionHost; spacing: 2 }
}
