import QtQuick
import QtQuick.Layouts
import FLiNGDownloader

/**
 * AppStatusBar - 应用状态栏
 */
Rectangle {
    id: statusBar
    
    property string message: qsTr("就绪")
    property int timeout: 0
    
    implicitHeight: 28
    color: ThemeProvider.backgroundColor
    
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: ThemeProvider.borderColor
    }
    
    Text {
        anchors.left: parent.left
        anchors.leftMargin: ThemeProvider.spacingMedium
        anchors.verticalCenter: parent.verticalCenter
        text: message
        font.pixelSize: ThemeProvider.fontSizeMedium
        font.bold: true
        color: ThemeProvider.textSecondary
    }
    
    Timer {
        id: clearTimer
        interval: timeout
        running: timeout > 0
        repeat: false
        onTriggered: message = qsTr("就绪")
    }
    
    function showMessage(msg, timeoutMs) {
        message = msg
        timeout = timeoutMs || 0
        if (timeout > 0) {
            clearTimer.restart()
        }
    }
}
