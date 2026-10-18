import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0

Page {
    id: page
    allowedOrientations: Orientation.All
    property bool justCopied: false

    function expiryHours() {
        switch (expiry.currentIndex) {
        case 0: return 1
        case 1: return 12
        case 2: return 24
        case 3: return 72
        default: return 24
        }
    }

    Component {
        id: anyFilePicker
        FilePickerPage {
            onSelectedContentPropertiesChanged: {
                if (selectedContentProperties && selectedContentProperties.filePath)
                    uploader.chooseFile(selectedContentProperties.filePath)
            }
        }
    }

    Timer {
        id: copiedTimer
        interval: 2500
        onTriggered: page.justCopied = false
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge
        clip: true

        PullDownMenu {
            MenuItem {
                text: qsTr("About LinkDrop")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
            MenuItem {
                text: qsTr("Choose file")
                enabled: !uploader.busy
                onClicked: pageStack.push(anyFilePicker)
            }
        }

        Column {
            id: content
            width: page.width
            spacing: Theme.paddingLarge

            PageHeader { title: qsTr("LinkDrop") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                text: qsTr("Share a file through a simple HTTPS link.")
                wrapMode: Text.WordWrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
            }

            SectionHeader { text: qsTr("File") }

            ValueButton {
                width: parent.width
                enabled: !uploader.busy
                label: qsTr("Choose file")
                value: uploader.hasFile ? uploader.selectedFileName : qsTr("Tap to browse")
                description: uploader.hasFile ? uploader.selectedFileSize : qsTr("Any file type")
                onClicked: pageStack.push(anyFilePicker)
            }

            SectionHeader { text: qsTr("Link duration") }

            ComboBox {
                id: expiry
                width: parent.width
                enabled: !uploader.busy
                label: qsTr("Delete after")
                currentIndex: 2
                menu: ContextMenu {
                    MenuItem { text: qsTr("1 hour") }
                    MenuItem { text: qsTr("12 hours") }
                    MenuItem { text: qsTr("24 hours") }
                    MenuItem { text: qsTr("72 hours") }
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.WordWrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: qsTr("Litterbox: up to 1 GB. Links expire automatically.")
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !uploader.busy
                enabled: uploader.hasFile
                text: uploader.phase === "done" ? qsTr("Upload again") : qsTr("Upload file")
                onClicked: {
                    page.justCopied = false
                    uploader.upload(page.expiryHours())
                }
            }

            Column {
                width: parent.width
                visible: uploader.busy
                spacing: Theme.paddingMedium

                ProgressBar {
                    width: parent.width
                    minimumValue: 0
                    maximumValue: 100
                    value: uploader.progress * 100
                    valueText: Math.floor(uploader.progress * 100) + "%"
                    label: uploader.status
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                    text: qsTr("Keep the app open until the server confirms the upload.")
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Cancel upload")
                    onClicked: uploader.cancel()
                }
            }

            Column {
                width: parent.width
                visible: uploader.phase === "done"
                spacing: Theme.paddingMedium

                SectionHeader { text: qsTr("Ready to share") }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    text: qsTr("Upload complete. Copy this link into IRC or any message.")
                    color: Theme.highlightColor
                    wrapMode: Text.WordWrap
                }

                TextArea {
                    width: parent.width
                    label: qsTr("Download link")
                    readOnly: true
                    text: uploader.shareUrl
                    wrapMode: Text.WrapAnywhere
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Copy link")
                    onClicked: {
                        Clipboard.text = uploader.shareUrl
                        page.justCopied = true
                        copiedTimer.restart()
                    }
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    visible: page.justCopied
                    text: qsTr("Copied to clipboard")
                    color: Theme.highlightColor
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            Column {
                width: parent.width
                visible: uploader.phase === "error" || uploader.phase === "cancelled"
                spacing: Theme.paddingSmall

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    color: uploader.phase === "error" ? Theme.errorColor : Theme.secondaryColor
                    text: uploader.status
                    wrapMode: Text.WordWrap
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    visible: uploader.errorMessage.length > 0
                    text: uploader.errorMessage
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
                text: qsTr("Not end-to-end encrypted. Anyone with the link can access the file; the hosting service may also access its contents.")
            }
        }
        VerticalScrollDecorator {}
    }
}
