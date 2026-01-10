import QtQuick
import QtQuick.Controls.Basic
import FLiNGDownloader

/**
 * StyledButton - 统一样式按钮组件
 * 支持主题切换，自动应用 ThemeProvider 中的样式
 */
Button {
    id: control
    
    // 按钮类型: "primary", "secondary", "success", "danger", "info"
    property string buttonType: "primary"
    property bool rounded: true
    
    implicitWidth: Math.max(80, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: 36
    
    leftPadding: 16
    rightPadding: 16
    topPadding: 8
    bottomPadding: 8
    
    font.pixelSize: ThemeProvider.fontSizeMedium
    font.bold: true
    
    // 根据类型获取背景色
    readonly property color typeColor: {
        switch(buttonType) {
            case "primary": return ThemeProvider.primaryColor
            case "secondary": return ThemeProvider.secondaryColor
            case "success": return ThemeProvider.successColor
            case "danger": return ThemeProvider.errorColor
            case "info": return ThemeProvider.infoColor
            default: return ThemeProvider.primaryColor
        }
    }
    
    // 计算悬停/按下状态的颜色
    readonly property color hoverTypeColor: Qt.darker(typeColor, 1.1)
    readonly property color pressedTypeColor: Qt.darker(typeColor, 1.2)
    
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? "white" : ThemeProvider.textDisabled
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    
    background: Rectangle {
        implicitWidth: 80
        implicitHeight: 36
        radius: rounded ? ThemeProvider.radiusMedium : 0
        color: {
            if (!control.enabled) return ThemeProvider.borderColor
            if (control.pressed) return pressedTypeColor
            if (control.hovered) return hoverTypeColor
            return typeColor
        }
        
        Behavior on color {
            ColorAnimation { duration: 100 }
        }
    }
}
