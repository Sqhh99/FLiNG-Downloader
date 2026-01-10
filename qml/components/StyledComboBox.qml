import QtQuick
import QtQuick.Controls.Basic
import FLiNGDownloader

/**
 * StyledComboBox - 统一样式下拉框组件
 */
ComboBox {
    id: control
    
    implicitWidth: 150
    implicitHeight: 36
    
    leftPadding: 12
    rightPadding: 36
    
    font.pixelSize: ThemeProvider.fontSizeMedium
    
    delegate: ItemDelegate {
        width: control.width
        height: 36
        
        contentItem: Text {
            text: modelData
            color: highlighted ? ThemeProvider.primaryColor : ThemeProvider.textPrimary
            font: control.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        
        background: Rectangle {
            color: highlighted ? ThemeProvider.hoverColor : "transparent"
        }
        
        highlighted: control.highlightedIndex === index
    }
    
    indicator: Canvas {
        id: canvas
        x: control.width - width - 12
        y: (control.height - height) / 2
        width: 12
        height: 8
        contextType: "2d"
        
        Connections {
            target: control
            function onPressedChanged() { canvas.requestPaint() }
        }
        
        Connections {
            target: ThemeProvider
            function onCurrentThemeChanged() { canvas.requestPaint() }
        }
        
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.moveTo(0, 0)
            ctx.lineTo(width, 0)
            ctx.lineTo(width / 2, height)
            ctx.closePath()
            ctx.fillStyle = control.pressed ? ThemeProvider.primaryColor : ThemeProvider.textSecondary
            ctx.fill()
        }
    }
    
    contentItem: Text {
        leftPadding: 0
        rightPadding: control.indicator.width + 12
        text: control.displayText
        font: control.font
        color: control.enabled ? ThemeProvider.textPrimary : ThemeProvider.textDisabled
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    
    background: Rectangle {
        implicitWidth: 150
        implicitHeight: 36
        radius: ThemeProvider.radiusMedium
        color: ThemeProvider.surfaceColor
        border.width: 1
        border.color: control.pressed ? ThemeProvider.primaryColor : ThemeProvider.borderColor
        
        Behavior on border.color {
            ColorAnimation { duration: 100 }
        }
    }
    
    popup: Popup {
        y: control.height + 2
        width: control.width
        implicitHeight: contentItem.implicitHeight + 2
        padding: 1
        
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        
        background: Rectangle {
            radius: ThemeProvider.radiusMedium
            color: ThemeProvider.surfaceColor
            border.width: 1
            border.color: ThemeProvider.borderColor
            

        }
    }
}
