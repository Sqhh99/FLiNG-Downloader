import QtQuick
import QtQuick.Controls.Basic
import FLiNGDownloader

/**
 * IconButton - 可复用的图标按钮组件
 * 用于列表项操作和标题栏按钮
 */
Rectangle {
    id: iconButton
    
    property string iconSource: ""
    property string tooltip: ""
    property int iconSize: 16
    property bool hovered: mouseArea.containsMouse
    property bool enabled: true
    property color normalColor: "transparent"
    property color hoverColor: ThemeProvider.hoverColor
    property color pressedColor: ThemeProvider.selectedColor
    property color iconColor: ThemeProvider.textPrimary
    property color iconHoverColor: ThemeProvider.primaryColor
    
    signal clicked()
    
    width: iconSize + 12
    height: iconSize + 12
    radius: ThemeProvider.radiusSmall
    color: {
        if (!enabled) return normalColor
        if (mouseArea.pressed) return pressedColor
        if (hovered) return hoverColor
        return normalColor
    }
    opacity: enabled ? 1.0 : 0.5
    
    Image {
        id: icon
        anchors.centerIn: parent
        width: iconButton.iconSize
        height: iconButton.iconSize
        source: iconButton.iconSource
        sourceSize: Qt.size(iconButton.iconSize, iconButton.iconSize)
        fillMode: Image.PreserveAspectFit
        smooth: true
        antialiasing: true
    }
    
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        
        onClicked: {
            if (iconButton.enabled) {
                iconButton.clicked()
            }
        }
    }
    
    ToolTip {
        visible: hovered && tooltip.length > 0
        text: tooltip
        delay: 500
        
        background: Rectangle {
            color: ThemeProvider.surfaceColor
            border.color: ThemeProvider.borderColor
            radius: ThemeProvider.radiusSmall
        }
        
        contentItem: Text {
            text: iconButton.tooltip
            font.pixelSize: ThemeProvider.fontSizeSmall
            color: ThemeProvider.textPrimary
        }
    }
}
