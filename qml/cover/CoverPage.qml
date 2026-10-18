import QtQuick 2.6
import Sailfish.Silica 1.0

CoverBackground {
    Column {
        width: parent.width - 2 * Theme.paddingLarge
        anchors.centerIn: parent
        spacing: Theme.paddingMedium

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Theme.iconSizeLarge
            height: width
            sourceSize.width: width
            sourceSize.height: height
            source: "/usr/share/icons/hicolor/172x172/apps/harbour-linkdrop.png"
            fillMode: Image.PreserveAspectFit
        }

        Label {
            width: parent.width
            text: qsTr("LinkDrop")
            horizontalAlignment: Text.AlignHCenter
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeLarge
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSizeSmall
            text: uploader.phase === "uploading"
                  ? qsTr("Uploading %1%").arg(Math.floor(uploader.progress * 100))
                  : uploader.phase === "done" ? qsTr("Link ready")
                  : uploader.phase === "error" ? qsTr("Upload failed")
                  : uploader.hasFile ? qsTr("File selected") : qsTr("Choose a file")
        }

        ProgressBar {
            width: parent.width
            visible: uploader.busy
            minimumValue: 0
            maximumValue: 100
            value: uploader.progress * 100
        }
    }
}
