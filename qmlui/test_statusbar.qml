import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.0

ApplicationWindow {
    id: window
    visible: true
    width: 800
    height: 600
    title: "QLC+ Status Bar Test"

    // Mock properties to simulate the qlcplus object
    property string programStatus: "Ready"
    property string autosaveStatus: "Ready"
    property string operatingMode: "Edit"
    property bool docModified: false

    // Timer to simulate status changes
    Timer {
        interval: 3000
        running: true
        repeat: true
        onTriggered: {
            // Cycle through different statuses
            if (window.programStatus === "Ready") {
                window.programStatus = "Loading"
                window.autosaveStatus = "Saving..."
            } else if (window.programStatus === "Loading") {
                window.programStatus = "Ready"
                window.autosaveStatus = "Saved 1m ago"
                window.operatingMode = window.operatingMode === "Edit" ? "Operate" : "Edit"
                window.docModified = !window.docModified
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#2a2a2a"

        // Main content area
        Rectangle {
            anchors.fill: parent
            anchors.bottomMargin: statusBar.height
            color: "#3a3a3a"
            
            Text {
                anchors.centerIn: parent
                text: "QLC+ Main Application Area\n\nStatus bar is shown at the bottom"
                color: "white"
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
            }
        }

        // Status Bar
        Rectangle {
            id: statusBar
            height: 32
            width: parent.width
            anchors.bottom: parent.bottom
            color: "#404040"
            border.width: 1
            border.color: "#606060"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 5
                spacing: 20

                // Program Status
                Row {
                    spacing: 5
                    
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Status:"
                        color: "white"
                        font.bold: true
                    }
                    
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: window.programStatus
                        color: {
                            switch(window.programStatus) {
                                case "Ready": return "white"
                                case "Loading": return "orange"
                                default: return "white"
                            }
                        }
                    }
                }

                // Separator
                Rectangle {
                    width: 1
                    height: parent.height * 0.6
                    color: "#606060"
                }

                // Autosave Status
                Row {
                    spacing: 5
                    
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Autosave:"
                        color: "white"
                        font.bold: true
                    }
                    
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: window.autosaveStatus
                        color: {
                            switch(window.autosaveStatus) {
                                case "Ready": return "green"
                                case "Disabled": return "red"
                                default: return "white"
                            }
                        }
                    }
                }

                // Separator
                Rectangle {
                    width: 1
                    height: parent.height * 0.6
                    color: "#606060"
                }

                // Operating Mode
                Row {
                    spacing: 5
                    
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Mode:"
                        color: "white"
                        font.bold: true
                    }
                    
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: window.operatingMode
                        color: {
                            switch(window.operatingMode) {
                                case "Edit": return "blue"
                                case "Operate": return "green"
                                default: return "white"
                            }
                        }
                    }
                }

                // Spacer to push everything to the left
                Item {
                    Layout.fillWidth: true
                }

                // Document Modified Indicator
                Row {
                    spacing: 5
                    visible: window.docModified
                    
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        color: "orange"
                        radius: 2
                        
                        Text {
                            anchors.centerIn: parent
                            text: "!"
                            color: "white"
                            font.bold: true
                            font.pixelSize: 12
                        }
                    }
                    
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Modified"
                        color: "orange"
                    }
                }
            }
        }
    }
}
