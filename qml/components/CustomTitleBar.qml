import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FLiNGDownloader

/**
 * CustomTitleBar - 自定义无边框窗口标题栏
 * 包含应用标题和窗口控制按钮
 */
Rectangle {
    id: titleBar
    
    property string title: ""
    property var targetWindow: null
    property bool maximized: targetWindow ? (targetWindow.visibility === Window.Maximized) : false
    property int activeDownloads: 0  // 活动下载数
    
    signal minimizeClicked()
    signal maximizeClicked()
    signal closeClicked()
    signal settingsClicked()
    signal downloadClicked()  // 下载列表按钮
    
    height: 40
    color: ThemeProvider.surfaceColor
    
    // 底部边框
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: ThemeProvider.borderColor
    }
    
    // 拖动区域
    MouseArea {
        id: dragArea
        anchors.fill: parent
        anchors.rightMargin: windowControls.width
        
        property point clickPos: Qt.point(0, 0)
        
        onPressed: function(mouse) {
            clickPos = Qt.point(mouse.x, mouse.y)
        }
        
        onPositionChanged: function(mouse) {
            if (pressed && targetWindow) {
                var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                if (maximized) {
                    // 从最大化状态拖动时还原窗口
                    targetWindow.showNormal()
                }
                targetWindow.x += delta.x
                targetWindow.y += delta.y
            }
        }
        
        onDoubleClicked: {
            if (maximized) {
                targetWindow.showNormal()
            } else {
                targetWindow.showMaximized()
            }
        }
    }
    
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 5
        spacing: 0
        
        // 应用图标
        Image {
            source: ThemeProvider.assetUrl("icons/app_icon.png")
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
        }
        
        // 标题
        Text {
            text: titleBar.title
            font.pixelSize: ThemeProvider.fontSizeMedium
            font.bold: true
            color: ThemeProvider.textPrimary
            Layout.fillWidth: true
            Layout.leftMargin: 10
            elide: Text.ElideRight
        }
        
        // 下载按钮（带徽章）
        Item {
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            
            IconButton {
                anchors.centerIn: parent
                iconSource: ThemeProvider.assetUrl("icons/download.png")
                tooltip: qsTr("下载列表")
                iconSize: 16
                onClicked: downloadClicked()
            }
            
            // 活动下载数徽章
            Rectangle {
                visible: activeDownloads > 0
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 2
                anchors.rightMargin: 2
                width: 16
                height: 16
                radius: 8
                color: ThemeProvider.dangerColor
                
                Text {
                    anchors.centerIn: parent
                    text: activeDownloads > 9 ? "9+" : activeDownloads.toString()
                    font.pixelSize: 10
                    font.bold: true
                    color: "white"
                }
            }
        }
        
        // 设置按钮
        IconButton {
            iconSource: ThemeProvider.assetUrl("icons/settings.png")
            tooltip: qsTr("设置")
            iconSize: 16
            onClicked: settingsClicked()
        }
        
        // 窗口控制按钮
        Row {
            id: windowControls
            Layout.alignment: Qt.AlignRight
            spacing: 0
            
            // 最小化
            Rectangle {
                width: 46
                height: 40
                color: minimizeArea.containsMouse ? ThemeProvider.hoverColor : "transparent"
                
                Image {
                    anchors.centerIn: parent
                    source: ThemeProvider.assetUrl("icons/minimize.png")
                    width: 16
                    height: 16
                    sourceSize: Qt.size(16, 16)
                }
                
                MouseArea {
                    id: minimizeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (targetWindow) targetWindow.showMinimized()
                        minimizeClicked()
                    }
                }
            }
            
            // 最大化/还原
            Rectangle {
                width: 46
                height: 40
                color: maximizeArea.containsMouse ? ThemeProvider.hoverColor : "transparent"
                
                Image {
                    anchors.centerIn: parent
                    source: maximized ? ThemeProvider.assetUrl("icons/maximize_restoration.png") : ThemeProvider.assetUrl("icons/maximize.png")
                    width: 16
                    height: 16
                    sourceSize: Qt.size(16, 16)
                }
                
                MouseArea {
                    id: maximizeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (targetWindow) {
                            if (maximized) {
                                targetWindow.showNormal()
                            } else {
                                targetWindow.showMaximized()
                            }
                        }
                        maximizeClicked()
                    }
                }
            }
            
            // 关闭
            Rectangle {
                width: 46
                height: 40
                color: closeArea.containsMouse ? "#E81123" : "transparent"
                
                Image {
                    anchors.centerIn: parent
                    source: ThemeProvider.assetUrl("icons/exit.png")
                    width: 16
                    height: 16
                    sourceSize: Qt.size(16, 16)
                }
                
                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (targetWindow) targetWindow.close()
                        closeClicked()
                    }
                }
            }
        }
    }
}
