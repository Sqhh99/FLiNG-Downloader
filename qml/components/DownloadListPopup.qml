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
    
    // 格式化文件大小
    function formatFileSize(bytes) {
        if (bytes <= 0) return "0 B"
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(2) + " MB"
        return (bytes / (1024 * 1024 * 1024)).toFixed(2) + " GB"
    }
    
    // 格式化下载速度
    function formatSpeed(bytesPerSec) {
        if (bytesPerSec <= 0) return ""
        if (bytesPerSec < 1024) return bytesPerSec + " B/s"
        if (bytesPerSec < 1024 * 1024) return (bytesPerSec / 1024).toFixed(1) + " KB/s"
        if (bytesPerSec < 1024 * 1024 * 1024) return (bytesPerSec / (1024 * 1024)).toFixed(2) + " MB/s"
        return (bytesPerSec / (1024 * 1024 * 1024)).toFixed(2) + " GB/s"
    }
    
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
        opacity: 1.0

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
                id: delegateRoot
                width: downloadListView.width
                height: 70
                color: index % 2 === 0 ? "transparent" : ThemeProvider.alternateRowColor
                radius: ThemeProvider.radiusSmall

                // 是否为不确定进度（服务器未返回文件大小）
                readonly property bool indeterminate: (modelData.status === "downloading") && (modelData.progress < 0)
                
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
                                if (modelData.status === "queued") return qsTr("队列中")
                                if (modelData.status === "downloading") return qsTr("下载中")
                                if (modelData.status === "paused") return qsTr("已暂停")
                                if (modelData.status === "completed") return qsTr("已完成")
                                if (modelData.status === "failed") return qsTr("失败")
                                if (modelData.status === "canceled") return qsTr("已取消")
                                return ""
                            }
                            font.pixelSize: ThemeProvider.fontSizeSmall
                            color: {
                                if (modelData.status === "completed") return ThemeProvider.successColor
                                if (modelData.status === "failed") return ThemeProvider.dangerColor
                                if (modelData.status === "canceled") return ThemeProvider.textSecondary
                                if (modelData.status === "paused") return ThemeProvider.warningColor
                                return ThemeProvider.textSecondary
                            }
                        }
                    }
                    
                    // 进度条
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 6

                        // 正常进度条（已知大小）
                        ProgressBar {
                            id: progressBar
                            anchors.fill: parent
                            visible: !delegateRoot.indeterminate
                            value: (modelData.progress >= 0) ? modelData.progress : 0

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
                                        if (modelData.status === "paused") return ThemeProvider.warningColor
                                        return ThemeProvider.primaryColor
                                    }
                                }
                            }
                        }

                        // 不确定进度条动画（未知大小）
                        Rectangle {
                            id: indeterminateBg
                            anchors.fill: parent
                            radius: 3
                            color: ThemeProvider.backgroundColor
                            visible: delegateRoot.indeterminate
                            clip: true

                            Rectangle {
                                id: indeterminateBar
                                width: parent.width * 0.3
                                height: parent.height
                                radius: 3
                                color: ThemeProvider.primaryColor

                                SequentialAnimation on x {
                                    running: delegateRoot.indeterminate
                                    loops: Animation.Infinite
                                    NumberAnimation {
                                        from: -indeterminateBar.width
                                        to: indeterminateBg.width
                                        duration: 1500
                                        easing.type: Easing.InOutQuad
                                    }
                                }
                            }
                        }
                    }
                    
                    // 操作按钮
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: ThemeProvider.spacingSmall
                        
                        // 进度文本：大小 + 百分比 + 速度
                        Text {
                            text: {
                                var status = modelData.status
                                if (status === "downloading" || status === "paused") {
                                    var received = modelData.bytesReceived || 0
                                    var total = modelData.bytesTotal || 0
                                    var result = formatFileSize(received)
                                    if (total > 0) {
                                        var pct = Math.round((modelData.progress || 0) * 100)
                                        result += " / " + formatFileSize(total) + "  " + pct + "%"
                                    }
                                    if (status === "downloading") {
                                        var spd = modelData.speed || 0
                                        if (spd > 0) {
                                            result += "  " + formatSpeed(spd)
                                        }
                                    }
                                    return result
                                }
                                if (status === "completed") {
                                    var fileSize = modelData.bytesTotal || modelData.bytesReceived || 0
                                    if (fileSize > 0) {
                                        return formatFileSize(fileSize)
                                    }
                                }
                                if (status === "failed" && modelData.errorMessage) {
                                    return modelData.errorMessage
                                }
                                return ""
                            }
                            font.pixelSize: ThemeProvider.fontSizeSmall
                            color: ThemeProvider.textSecondary
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        
                        // 暂停/继续按钮
                        IconButton {
                            visible: modelData.status === "downloading" || modelData.status === "paused" || modelData.status === "queued"
                            iconSource: (modelData.status === "paused" || modelData.status === "queued")
                                        ? ThemeProvider.assetUrl("icons/step-forward.png")
                                        : ThemeProvider.assetUrl("icons/pause.png")
                            iconSize: 14
                            tooltip: {
                                if (modelData.status === "paused") return qsTr("继续")
                                if (modelData.status === "queued") return qsTr("继续")
                                return qsTr("暂停")
                            }
                            onClicked: {
                                if (modelData.status === "paused" || modelData.status === "queued") {
                                    resumeDownload(index)
                                } else {
                                    pauseDownload(index)
                                }
                            }
                        }
                        
                        // 重试按钮（失败任务）
                        IconButton {
                            visible: modelData.status === "failed"
                            iconSource: ThemeProvider.assetUrl("icons/step-forward.png")
                            iconSize: 14
                            tooltip: qsTr("重试")
                            onClicked: resumeDownload(index)
                        }
                        
                        // 取消按钮
                        IconButton {
                            visible: modelData.status === "downloading" || modelData.status === "paused" || modelData.status === "queued"
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
                            visible: modelData.status === "completed" || modelData.status === "failed" || modelData.status === "canceled"
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
