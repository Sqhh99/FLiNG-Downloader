import QtQuick
import QtQuick.Controls.Basic
import FLiNGDownloader

/**
 * StyledTextField - 统一样式输入框组件
 */
TextField {
    id: control
    
    property bool showClearButton: true
    
    implicitWidth: 200
    implicitHeight: 36
    
    leftPadding: 12
    rightPadding: showClearButton ? 36 : 12
    topPadding: 8
    bottomPadding: 8
    
    font.pixelSize: ThemeProvider.fontSizeMedium
    color: ThemeProvider.textPrimary
    placeholderTextColor: ThemeProvider.textDisabled
    selectionColor: ThemeProvider.primaryColor
    selectedTextColor: "white"
    
    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 36
        radius: ThemeProvider.radiusMedium
        color: ThemeProvider.surfaceColor
        border.width: 1
        border.color: control.activeFocus ? ThemeProvider.primaryColor : ThemeProvider.borderColor
        
        Behavior on border.color {
            ColorAnimation { duration: 100 }
        }
    }
    
    // 清除按钮
    Rectangle {
        visible: showClearButton && control.text.length > 0 && control.activeFocus
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: 20
        height: 20
        radius: 10
        color: clearMouseArea.containsMouse ? ThemeProvider.borderColor : "transparent"
        
        Text {
            anchors.centerIn: parent
            text: "×"
            font.pixelSize: 14
            font.bold: true
            color: ThemeProvider.textSecondary
        }
        
        MouseArea {
            id: clearMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: control.clear()
        }
    }
}
