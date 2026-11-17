import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

ApplicationWindow {
    id: root

    width: Math.min(1440, Screen.desktopAvailableWidth - 32)
    height: Math.min(900, Screen.desktopAvailableHeight - 48)
    minimumWidth: 1280
    minimumHeight: 640
    visible: true
    title: "StaxStudio"
    color: "#0F171A"

    property bool reducedMotion: !Qt.styleHints.cursorFlashTime
    Rectangle {
        anchors.fill: parent
        z: 100
        color: "#0F171A"
        visible: opacity > 0
        enabled: false
        Text { anchors.centerIn: parent; text: "StaxStudio"; color: "#B7E65C"; font.pixelSize: 32; font.weight: Font.DemiBold }
        NumberAnimation on opacity { from: 1; to: 0; duration: root.reducedMotion ? 0 : 280; running: true }
    }
    function pageComponent(pageName) {
        if (pageName === "record") {
            return recordPage
        }
        if (pageName === "studio") {
            return studioPage
        }
        if (pageName === "stream") {
            return streamPage
        }
        if (pageName === "library") {
            return libraryPage
        }
        if (pageName === "settings") {
            return settingsPage
        }
        return homePage
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        NavigationRail {
            Layout.fillHeight: true
            Layout.preferredWidth: 184
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0F171A"

            Loader {
                anchors.fill: parent
                sourceComponent: root.pageComponent(appController.activePage)
            }
        }
    }

    Component {
        id: homePage
        Home { }
    }

    Component {
        id: recordPage
        Record { }
    }

    Component {
        id: studioPage
        Studio { }
    }

    Component {
        id: streamPage
        Stream { }
    }

    Component {
        id: libraryPage
        Library { }
    }

    Component {
        id: settingsPage
        Settings { }
    }
}

