import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "twitchControls"
    property var destinations: studioController.streamingProviders()
    property var destination: destinations.find(d => d.provider === "twitch")
    property bool connected: destination ? destination.connected : false
    property var categories: studioController.twitchCategories
    property var metadata: studioController.activeStreamMetadata

    function loadForm() {
        metadata = studioController.activeStreamMetadata
        titleField.text = metadata.title || ""
        categoryIdField.text = metadata.categoryId || ""
        languageField.text = metadata.language || "en"
        tagsField.text = (metadata.tags || []).join(", ")
    }

    function saveForm() {
        const tags = tagsField.text.split(",").map(t => t.trim()).filter(t => t.length > 0)
        studioController.setActiveStreamMetadata({
            title: titleField.text.trim(), categoryId: categoryIdField.text.trim(),
            language: languageField.text.trim(), tags: tags
        })
    }

    Component.onCompleted: loadForm()
    onVisibleChanged: if (visible) Qt.callLater(root.loadForm)
    Connections {
        target: studioController
        function onProjectChanged() {
            root.destinations = studioController.streamingProviders()
            root.categories = studioController.twitchCategories
        }
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scroll.availableWidth - 4
            spacing: 10

            Text { text: "Account"; color: "#EAF0F1"; font.pixelSize: 14; font.weight: Font.DemiBold }
            Text {
                Layout.fillWidth: true
                text: root.connected ? root.destination.accountName : "Twitch is not connected"
                color: root.connected ? "#B7E65C" : "#8FA3A7"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
            RowLayout {
                Layout.fillWidth: true
                StudioButton { text: root.connected ? "Reconnect" : "Connect Twitch"; onClicked: studioController.connectTwitch() }
                StudioButton { text: "Disconnect"; visible: root.connected; onClicked: studioController.disconnectTwitch() }
                StudioButton {
                    text: "Cancel"
                    visible: studioController.twitchUserCode.length > 0
                    onClicked: studioController.cancelTwitchConnection()
                }
            }
            Text {
                Layout.fillWidth: true
                visible: studioController.twitchUserCode.length > 0
                text: "Enter code " + studioController.twitchUserCode + " in the Twitch browser page"
                color: "#B7E65C"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }
            Rectangle { Layout.fillWidth: true; Layout.topMargin: 6; height: 1; color: "#34474A" }

            Text { text: "Channel"; color: "#EAF0F1"; font.pixelSize: 14; font.weight: Font.DemiBold }
            Text { text: "Title"; color: "#B6C6C9"; font.pixelSize: 12 }
            StudioTextField { id: titleField; Layout.fillWidth: true; placeholderText: "Stream title" }
            Text { text: "Category / Game"; color: "#B6C6C9"; font.pixelSize: 12 }
            RowLayout {
                Layout.fillWidth: true
                StudioTextField {
                    id: categorySearch
                    Layout.fillWidth: true
                    placeholderText: "Search Twitch categories"
                    onAccepted: studioController.searchTwitchCategories(text)
                }
                StudioButton { text: "Search"; onClicked: studioController.searchTwitchCategories(categorySearch.text) }
            }
            Repeater {
                model: root.categories
                delegate: StudioButton {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.name
                    onClicked: {
                        categoryName.text = modelData.name
                        categoryIdField.text = modelData.id
                    }
                }
            }
            Text {
                id: categoryName
                Layout.fillWidth: true
                text: categoryIdField.text.length > 0 ? "Selected category ID: " + categoryIdField.text : "No category selected"
                color: "#93A8AC"
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            StudioTextField {
                id: categoryIdField
                visible: false
                text: root.metadata.categoryId || ""
            }
            Text { text: "Language"; color: "#B6C6C9"; font.pixelSize: 12 }
            StudioTextField { id: languageField; Layout.fillWidth: true; placeholderText: "en" }
            Text { text: "Tags"; color: "#B6C6C9"; font.pixelSize: 12 }
            StudioTextField { id: tagsField; Layout.fillWidth: true; placeholderText: "Comma-separated tags" }
            StudioCheckBox { id: recordBox; text: "Record Stream" }
            RowLayout {
                Layout.fillWidth: true
                StudioButton { text: "Save"; onClicked: root.saveForm() }
                Item { Layout.fillWidth: true }
                StudioButton {
                    text: studioController.twitchStreamActive ? "Stop Streaming" : "Go Live"
                    enabled: studioController.twitchStreamActive || root.connected
                    onClicked: {
                        if (studioController.twitchStreamActive) studioController.stopTwitchStream()
                        else { root.saveForm(); studioController.goLiveTwitch(recordBox.checked) }
                    }
                }
            }
            Item { Layout.preferredHeight: 8 }
        }
    }
}
