import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

ApplicationWindow {
    id: root

    width: Math.min(1440, Screen.desktopAvailableWidth - 32)
    height: Math.min(900, Screen.desktopAvailableHeight - 48)
    minimumWidth: 1280
    minimumHeight: 760
    visible: true
    title: "StaxStudio"
    color: "#F5F7F8"

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
            Layout.preferredWidth: 210
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#F5F7F8"

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
