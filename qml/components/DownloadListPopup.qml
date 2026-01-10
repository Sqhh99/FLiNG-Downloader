import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FLiNGDownloader

/**
 * DownloadListPopup - 浏览器式下载列表弹窗
 * 显示下载队列、进度条、暂停/继续/取消功能
 */
Popup {
    id: downloadPopup
    
    property var downloadItems: []  // [{name, progress, status, filePath}]
    property int activeDownloads: 0
    
    signal pauseDownload(int index)
    signal resumeDownload(int index)
    signal cancelDownload(int index)
    signal openFolder(int index)
    signal removeFromList(int index)
    
    width: 400
    height: Math.min(450, downloadItems.length * 80 + 100)
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    
    x: parent.width - width - 10
    y: 50
    
    background: Rectangle {
        color: ThemeProvider.surfaceColor
        border.color: ThemeProvider.borderColor
        border.width: 1
        radius: ThemeProvider.radiusMedium
        opacity: 1.0  // 确保不透明
        
        // 阴影效果 - 使用多层叠加增强
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            z: -1
            radius: ThemeProvider.radiusMedium + 4
            color: "#40000000"
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            z: -1
            radius: ThemeProvider.radiusMedium + 2
            color: "#30000000"
        }
    }
    
    contentItem: ColumnLayout {
        spacing: ThemeProvider.spacingMedium
        
        // 标题栏
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: ThemeProvider.spacingMedium
            
            Text {
                text: qsTr("下载列表")
                font.pixelSize: ThemeProvider.fontSizeTitle
                font.bold: true
                color: ThemeProvider.textPrimary
                Layout.fillWidth: true
            }
            
            Text {
                text: activeDownloads > 0 ? qsTr("正在下载 %1 项").arg(activeDownloads) : qsTr("无活动下载")
                font.pixelSize: ThemeProvider.fontSizeSmall
                color: ThemeProvider.textSecondary
            }
        }
        
        // 分割线
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: ThemeProvider.borderColor
        }
        
        // 下载列表
        ListView {
            id: downloadListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: ThemeProvider.spacingSmall
            clip: true
            
            model: downloadItems
            
            delegate: Rectangle {
                width: downloadListView.width
                height: 70
                color: index % 2 === 0 ? "transparent" : ThemeProvider.alternateRowColor
                radius: ThemeProvider.radiusSmall
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: ThemeProvider.spacingSmall
                    spacing: 4
                    
                    // 文件名和状态
                    RowLayout {
                        Layout.fillWidth: true
                        
                        Text {
                            text: modelData.fileName || modelData.name || qsTr("未知文件")
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textPrimary
                            Layout.fillWidth: true
                            Layout.preferredWidth: 250
                            elide: Text.ElideMiddle
                        }
                        
                        Text {
                            text: {
                                if (modelData.status === "downloading") return qsTr("下载中")
                                if (modelData.status === "paused") return qsTr("已暂停")
                                if (modelData.status === "completed") return qsTr("已完成")
                                if (modelData.status === "failed") return qsTr("失败")
                                return ""
                            }
                            font.pixelSize: ThemeProvider.fontSizeSmall
                            color: {
                                if (modelData.status === "completed") return ThemeProvider.successColor
                                if (modelData.status === "failed") return ThemeProvider.dangerColor
                                if (modelData.status === "paused") return ThemeProvider.warningColor
                                return ThemeProvider.textSecondary
                            }
                        }
                    }
                    
                    // 进度条
                    ProgressBar {
                        id: progressBar
                        Layout.fillWidth: true
                        Layout.preferredHeight: 6
                        value: modelData.progress || 0
                        
                        background: Rectangle {
                            radius: 3
                            color: ThemeProvider.backgroundColor
                        }
                        
                        contentItem: Item {
                            Rectangle {
                                width: progressBar.visualPosition * parent.width
                                height: parent.height
                                radius: 3
                                color: {
                                    if (modelData.status === "completed") return ThemeProvider.successColor
                                    if (modelData.status === "failed") return ThemeProvider.dangerColor
                                    return ThemeProvider.primaryColor
                                }
                            }
                        }
                    }
                    
                    // 操作按钮
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: ThemeProvider.spacingSmall
                        
                        // 进度文本
                        Text {
                            text: modelData.status === "downloading" 
                                  ? Math.round((modelData.progress || 0) * 100) + "%" 
                                  : ""
                            font.pixelSize: ThemeProvider.fontSizeSmall
                            color: ThemeProvider.textSecondary
                            Layout.fillWidth: true
                        }
                        
                        // 暂停/继续按钮
                        IconButton {
                            visible: modelData.status === "downloading" || modelData.status === "paused"
                            iconSource: modelData.status === "paused" 
                                        ? ThemeProvider.assetUrl("icons/download.png") 
                                        : ThemeProvider.assetUrl("icons/minimize.png")
                            iconSize: 14
                            tooltip: modelData.status === "paused" ? qsTr("继续") : qsTr("暂停")
                            onClicked: {
                                if (modelData.status === "paused") {
                                    resumeDownload(index)
                                } else {
                                    pauseDownload(index)
                                }
                            }
                        }
                        
                        // 取消按钮
                        IconButton {
                            visible: modelData.status === "downloading" || modelData.status === "paused"
                            iconSource: ThemeProvider.assetUrl("icons/exit.png")
                            iconSize: 14
                            tooltip: qsTr("取消")
                            onClicked: cancelDownload(index)
                        }
                        
                        // 打开文件夹按钮
                        IconButton {
                            visible: modelData.status === "completed"
                            iconSource: ThemeProvider.assetUrl("icons/folder.png")
                            iconSize: 14
                            tooltip: qsTr("打开文件夹")
                            onClicked: openFolder(index)
                        }
                        
                        // 删除条目按钮
                        IconButton {
                            visible: modelData.status === "completed" || modelData.status === "failed"
                            iconSource: ThemeProvider.assetUrl("icons/delete.png")
                            iconSize: 14
                            tooltip: qsTr("移除")
                            onClicked: removeFromList(index)
                        }
                    }
                }
            }
            
            // 空状态
            Text {
                visible: downloadItems.length === 0
                anchors.centerIn: parent
                text: qsTr("暂无下载任务")
                font.pixelSize: ThemeProvider.fontSizeMedium
                color: ThemeProvider.textDisabled
            }
        }
    }
}
