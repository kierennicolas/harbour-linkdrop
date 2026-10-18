import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader { title: qsTr("About LinkDrop") }

            SectionHeader { text: qsTr("What it does") }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.WordWrap
                text: qsTr("Choose a local file, upload it over HTTPS and copy its download link into IRC. No DCC or open inbound port is needed.")
            }

            SectionHeader { text: qsTr("Hosting") }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.WordWrap
                text: qsTr("Uploads go to Litterbox and expire after 1, 12, 24 or 72 hours (up to 1 GB). Litterbox is a third-party service with its own rules and availability.")
            }

            SectionHeader { text: qsTr("Important privacy difference") }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.WordWrap
                text: qsTr("LinkDrop is NOT an official wormhole.app client, and it does not use Wormhole's end-to-end encryption. HTTPS protects the file while it travels to the server, but the hosting provider can access the stored file. Do not use this for secrets.")
            }

            SectionHeader { text: qsTr("About this app") }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.WordWrap
                color: Theme.secondaryColor
                text: qsTr("Version %1. Native Sailfish Silica interface. Open source under the MIT licence; not affiliated with Litterbox, Catbox or Wormhole.").arg(appVersion)
            }
        }
        VerticalScrollDecorator {}
    }
}
