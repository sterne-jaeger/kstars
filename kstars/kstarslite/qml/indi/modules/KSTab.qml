import QtQuick 2.4
import QtQuick.Layouts 1.2
import QtQuick.Controls 1.4
import org.kde.kirigami 1.0 as Kirigami
import "../../constants" 1.0

Tab {
    id: tab
    anchors {
        fill: parent
        leftMargin: 5 * num.dp
        rightMargin: 5 * num.dp
        bottomMargin: 5 * num.dp
    }
    active: true

    property Item columnItem

    ScrollView {
        anchors.fill: parent
        horizontalScrollBarPolicy: Qt.ScrollBarAlwaysOff

        Component.onCompleted: {
            contentItem: column
        }

        Column {
            id: column
            anchors.leftMargin: 5 * num.dp

            Component.onCompleted: {
                tab.columnItem = this
            }
        }
    }
}