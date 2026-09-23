import QtQuick
import QtQuick.Controls

ComboBox {
    id: root
    implicitHeight: 34
    leftPadding: 11
    rightPadding: 34
    contentItem: Text { text: root.displayText; color: root.enabled ? "#E5EEEE" : "#65777C"; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
    indicator: StudioIcon { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; name: "chevron"; color: root.enabled ? "#B8C9CD" : "#607076" }
    background: Rectangle { radius: 4; color: root.hovered ? "#24343A" : "#1B292E"; border.color: root.activeFocus ? "#6C8F7D" : "#344850" }
    delegate: ItemDelegate {
        id: option
        required property int index
        required property string modelData
        width: root.popup.width
        height: 34
        highlighted: root.highlightedIndex === index
        contentItem: Text { text: option.modelData; color: option.highlighted ? "#F0F6F4" : "#BFCCCF"; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter; leftPadding: 10; elide: Text.ElideRight }
        background: Rectangle { color: option.highlighted || option.hovered ? "#2A4140" : (root.currentIndex === option.index ? "#223638" : "transparent"); radius: 3 }
    }
    popup: Popup {
        y: root.height + 4
        width: root.width
        implicitHeight: contentItem.implicitHeight + 8
        padding: 4
        contentItem: ListView { clip: true; implicitHeight: contentHeight; model: root.popup.visible ? root.delegateModel : null; currentIndex: root.highlightedIndex }
        background: Rectangle { color: "#18262B"; radius: 5; border.color: "#40565D" }
    }
}
