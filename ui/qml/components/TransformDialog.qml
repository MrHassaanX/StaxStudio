import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StudioDialog {
    id: root
    title: "Edit Transform"
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    property string sceneItemId: ""
    signal transformAccepted(var values)

    function openFor(itemId, values) {
        sceneItemId = itemId
        xField.text = values.x
        yField.text = values.y
        widthField.text = values.width
        heightField.text = values.height
        scaleXField.text = values.scaleX
        scaleYField.text = values.scaleY
        rotationField.text = values.rotation
        cropLeftField.text = values.cropLeft
        cropTopField.text = values.cropTop
        cropRightField.text = values.cropRight
        cropBottomField.text = values.cropBottom
        flipHorizontal.checked = values.flipHorizontal
        flipVertical.checked = values.flipVertical
        open()
    }

    onOpened: xField.forceActiveFocus()
    onAccepted: transformAccepted({
        "x": Number(xField.text), "y": Number(yField.text), "width": Number(widthField.text), "height": Number(heightField.text),
        "scaleX": Number(scaleXField.text), "scaleY": Number(scaleYField.text), "rotation": Number(rotationField.text),
        "cropLeft": Number(cropLeftField.text), "cropTop": Number(cropTopField.text), "cropRight": Number(cropRightField.text), "cropBottom": Number(cropBottomField.text),
        "flipHorizontal": flipHorizontal.checked, "flipVertical": flipVertical.checked
    })

    contentItem: ColumnLayout {
        implicitWidth: 390
        spacing: 8
        GridLayout {
            Layout.fillWidth: true
            columns: 4
            rowSpacing: 7
            columnSpacing: 7
            Text { text: "Position X"; color: "#AABCC0"; font.pixelSize: 11 }
            StudioTextField { id: xField; Layout.preferredWidth: 82; selectByMouse: true; validator: DoubleValidator {} }
            Text { text: "Position Y"; color: "#AABCC0"; font.pixelSize: 11 }
            StudioTextField { id: yField; Layout.preferredWidth: 82; selectByMouse: true; validator: DoubleValidator {} }
            Text { text: "Width"; color: "#AABCC0"; font.pixelSize: 11 }
            StudioTextField { id: widthField; Layout.preferredWidth: 82; selectByMouse: true; validator: DoubleValidator { bottom: 1 } }
            Text { text: "Height"; color: "#AABCC0"; font.pixelSize: 11 }
            StudioTextField { id: heightField; Layout.preferredWidth: 82; selectByMouse: true; validator: DoubleValidator { bottom: 1 } }
            Text { text: "Scale X"; color: "#AABCC0"; font.pixelSize: 11 }
            StudioTextField { id: scaleXField; Layout.preferredWidth: 82; selectByMouse: true; validator: DoubleValidator { bottom: 0.01 } }
            Text { text: "Scale Y"; color: "#AABCC0"; font.pixelSize: 11 }
            StudioTextField { id: scaleYField; Layout.preferredWidth: 82; selectByMouse: true; validator: DoubleValidator { bottom: 0.01 } }
            Text { text: "Rotation"; color: "#AABCC0"; font.pixelSize: 11 }
            StudioTextField { id: rotationField; Layout.preferredWidth: 82; selectByMouse: true; validator: DoubleValidator {} }
            Text { text: "Crop left"; color: "#AABCC0"; font.pixelSize: 11 }
            StudioTextField { id: cropLeftField; Layout.preferredWidth: 82; selectByMouse: true; validator: DoubleValidator { bottom: 0 } }
            Text { text: "Crop top"; color: "#AABCC0"; font.pixelSize: 11 }
            StudioTextField { id: cropTopField; Layout.preferredWidth: 82; selectByMouse: true; validator: DoubleValidator { bottom: 0 } }
            Text { text: "Crop right"; color: "#AABCC0"; font.pixelSize: 11 }
            StudioTextField { id: cropRightField; Layout.preferredWidth: 82; selectByMouse: true; validator: DoubleValidator { bottom: 0 } }
            Text { text: "Crop bottom"; color: "#AABCC0"; font.pixelSize: 11 }
            StudioTextField { id: cropBottomField; Layout.preferredWidth: 82; selectByMouse: true; validator: DoubleValidator { bottom: 0 } }
        }
        RowLayout {
            Layout.fillWidth: true
            CheckBox { id: flipHorizontal; text: "Flip horizontal" }
            CheckBox { id: flipVertical; text: "Flip vertical" }
        }
    }
}
