/*
  Q Light Controller Plus
  StatusBar.qml

  Copyright (c) Heikki Junnila
                Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

import QtQuick 2.0
import QtQuick.Layouts 1.1

import "."

Rectangle
{
    id: statusBar
    height: UISettings.iconSizeMedium
    color: UISettings.bgMedium
    border.width: 1
    border.color: UISettings.bgLight

    RowLayout
    {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 20

        // Program Status
        Row
        {
            spacing: 5
            
            RobotoText
            {
                anchors.verticalCenter: parent.verticalCenter
                label: qsTr("Status:")
                fontSize: UISettings.textSizeDefault
                fontBold: true
            }
            
            RobotoText
            {
                anchors.verticalCenter: parent.verticalCenter
                label: qlcplus.programStatus
                fontSize: UISettings.textSizeDefault
                color: {
                    switch(qlcplus.programStatus) {
                        case "Ready": return UISettings.fgMain
                        case "Loading": return "orange"
                        default: return UISettings.fgMain
                    }
                }
            }
        }

        // Separator
        Rectangle
        {
            width: 1
            height: parent.height * 0.6
            color: UISettings.bgLight
        }

        // Autosave Status
        Row
        {
            spacing: 5
            
            RobotoText
            {
                anchors.verticalCenter: parent.verticalCenter
                label: qsTr("Autosave:")
                fontSize: UISettings.textSizeDefault
                fontBold: true
            }
            
            RobotoText
            {
                anchors.verticalCenter: parent.verticalCenter
                label: qlcplus.autosaveStatus
                fontSize: UISettings.textSizeDefault
                color: {
                    switch(qlcplus.autosaveStatus) {
                        case "Ready": return "green"
                        case "Disabled": return "red"
                        default: return UISettings.fgMain
                    }
                }
            }
        }

        // Separator
        Rectangle
        {
            width: 1
            height: parent.height * 0.6
            color: UISettings.bgLight
        }

        // Operating Mode
        Row
        {
            spacing: 5
            
            RobotoText
            {
                anchors.verticalCenter: parent.verticalCenter
                label: qsTr("Mode:")
                fontSize: UISettings.textSizeDefault
                fontBold: true
            }
            
            RobotoText
            {
                anchors.verticalCenter: parent.verticalCenter
                label: qlcplus.operatingMode
                fontSize: UISettings.textSizeDefault
                color: {
                    switch(qlcplus.operatingMode) {
                        case "Edit": return "blue"
                        case "Operate": return "green"
                        default: return UISettings.fgMain
                    }
                }
            }
        }

        // Spacer to push everything to the left
        Item
        {
            Layout.fillWidth: true
        }

        // Document Modified Indicator
        Row
        {
            spacing: 5
            visible: qlcplus.docModified
            
            Image
            {
                anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/filesave.svg"
                height: UISettings.iconSizeDefault
                width: height
                sourceSize: Qt.size(width, height)
            }
            
            RobotoText
            {
                anchors.verticalCenter: parent.verticalCenter
                label: qsTr("Modified")
                fontSize: UISettings.textSizeDefault
                color: "orange"
            }
        }
    }
}
