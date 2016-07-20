import QtQuick 2.4
import QtQuick.Window 2.2
import "../modules"
import "../constants" 1.0
import QtQuick.Layouts 1.2
import QtQuick.Controls 1.4 as Controls
import org.kde.kirigami 1.0 as Kirigami

KSPage {
    id: indiPage
    title: "INDI Control Panel"
    objectName: "indiControlPanel"

    property bool connected: ClientManagerLite.connected

    Component.onCompleted: {
        ClientManagerLite.setHost("localhost", parseInt(7624))
    }

    onHeightChanged: {
        devicesPage.height = indiPage.height - devicesPage.y
    }

    onConnectedChanged: {
        if(!connected) {
            for(var i = 0; i < devicesModel.count; ++i) {
                devicesModel.get(i).panel.destroy()
            }
            devicesModel.clear()
            //showPage()
            showPassiveNotification("Disconnected from the server")
        }
    }
    contentItem: cPanelColumn

    ColumnLayout {
        id: cPanelColumn

        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            topMargin: 5 * num.dp
            leftMargin: 25 * num.dp
            rightMargin: 25 * num.dp
        }

        ColumnLayout {
            visible: !connected
            anchors {
                left: parent.left
                right: parent.right
            }

            Kirigami.Label {
                text: "INDI Host"
            }

            RowLayout {
                anchors {
                    left: parent.left
                    right: parent.right
                }

                Controls.TextField {
                    id:ipHost
                    placeholderText: "IP"
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: parent.width*0.8
                }

                Controls.TextField {
                    id:portHost
                    placeholderText: "Port"
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: parent.width*0.2
                }
            }
        }

        Kirigami.Label {
            id: connectedTo
            visible: connected
            text: "Connected to " + ClientManagerLite.connectedHost
        }

        Controls.Button {
            text: connected ? "Disconnect" : "Connect"
            onClicked: {
                if(!connected) {
                    if(ClientManagerLite.setHost(ipHost.text, parseInt(portHost.text))) {
                        showPassiveNotification("Successfully connected to the server")
                    } else {
                        showPassiveNotification("Couldn't connect to the server")
                    }
                } else {
                    ClientManagerLite.disconnectHost()
                }
                Qt.inputMethod.hide()
            }
        }

        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            visible : connected

            Component.onCompleted: {
                for(var i = 0; i < 100; ++i) {
                    console.log(height)
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1 * num.dp
                color: "gray"
            }

            Kirigami.Label {
                id: devicesLabel
                text: "Available Devices"
            }

            ListModel {
                id: devicesModel
            }

            Connections {
                target: ClientManagerLite
                onNewINDIDevice: {
                    var component = Qt.createComponent(Qt.resolvedUrl("./DevicePanel.qml"));
                    var devicePanel = component.createObject(pagesWindow);
                    devicePanel.title = deviceName
                    devicesModel.append({ name: deviceName, panel: devicePanel })
                }
                onRemoveINDIDevice: {
                    for(i = 0; i < devicesModel.count; ++i) {
                        if(devicesModel.get(i).name == deviceName) {
                            devicesModel.panel.destroy()
                            devicesModel.remove(i)
                        }
                    }
                }
            }
        }
    }

    Kirigami.ScrollablePage {
        id:devicesPage
        anchors {
            top: cPanelColumn.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            topMargin: 5 * num.dp
            leftMargin: 25 * num.dp
            rightMargin: 25 * num.dp
            bottomMargin: 25 * num.dp
        }

        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0

        ListView {
            id: list

            model: devicesModel
            delegate: Kirigami.BasicListItem {
                label: model.name
                enabled: true
                onClicked: {
                    model.panel.showPage(false)
                }
            }
        }

    }
}
