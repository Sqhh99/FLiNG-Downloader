import QtQuick
import QtQuick.Controls.Basic
import FLiNGDownloader

/**
 * ProgressIndicator - 下载进度条组件
 */
ProgressBar {
    id: control
    
    property string statusText: ""
    property bool showText: true
    
    implicitWidth: 200
    implicitHeight: showText ? 24 : 8
    
    background: Rectangle {
        implicitWidth: 200
        implicitHeight: showText ? 24 : 8
        radius: ThemeProvider.radiusMedium
        color: ThemeProvider.backgroundColor
        border.width: 1
        border.color: ThemeProvider.borderColor
    }
    
    contentItem: Item {
        implicitWidth: 200
        implicitHeight: showText ? 24 : 8
        
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: ThemeProvider.radiusMedium
            color: ThemeProvider.primaryColor
            
            Behavior on width {
                NumberAnimation { duration: 100 }
            }
        }
        
        Text {
            visible: showText
            anchors.centerIn: parent
            text: statusText.length > 0 ? statusText : Math.round(control.value * 100) + "%"
            font.pixelSize: ThemeProvider.fontSizeSmall
            font.bold: true
            color: control.value > 0.5 ? "white" : ThemeProvider.textPrimary
        }
    }
}
