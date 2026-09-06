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
    
    readonly property int rowHeight: 60

    width: 300
    height: downloadItems.length === 0
            ? 118
            : Math.min(268, 40 + downloadItems.length * rowHeight + 8)
    padding: 8
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    
    x: parent.width - width - 8
    y: 42
    
    background: Rectangle {
        color: ThemeProvider.surfaceColor
        border.color: ThemeProvider.borderColor
        border.width: 1
        radius: ThemeProvider.radiusMedium
    }
    
    contentItem: ColumnLayout {
        spacing: 6
        
        // 标题栏
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            
            Text {
                text: qsTr("下载列表")
                font.pixelSize: ThemeProvider.fontSizeMedium
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
            clip: true
            
            // 用条数做 model，进度刷新时复用行，避免进度条动画被重置
            model: downloadItems ? downloadItems.length : 0
            
            delegate: Rectangle {
                id: delegateRoot
                width: downloadListView.width
                height: downloadPopup.rowHeight
                color: index % 2 === 0 ? "transparent" : ThemeProvider.alternateRowColor
                radius: ThemeProvider.radiusSmall

                readonly property var item: {
                    var items = downloadPopup.downloadItems
                    if (!items || index < 0 || index >= items.length)
                        return ({})
                    return items[index]
                }

                // 下载中且没有总大小时走不确定进度动画
                readonly property bool indeterminate: {
                    if (item.status !== "downloading")
                        return false
                    if (typeof item.progress === "number" && item.progress < 0)
                        return true
                    return !(item.bytesTotal > 0)
                }
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 6
                    anchors.topMargin: 4
                    anchors.bottomMargin: 4
                    spacing: 2
                    
                    // 文件名和状态
                    RowLayout {
                        Layout.fillWidth: true
                        
                        Text {
                            text: item.fileName || item.name || qsTr("未知文件")
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textPrimary
                            Layout.fillWidth: true
                            Layout.preferredWidth: 170
                            elide: Text.ElideMiddle
                        }
                        
                        Text {
                            text: {
                                if (item.status === "queued") return qsTr("队列中")
                                if (item.status === "downloading") return qsTr("下载中")
                                if (item.status === "paused") return qsTr("已暂停")
                                if (item.status === "completed") return qsTr("已完成")
                                if (item.status === "failed") return qsTr("失败")
                                if (item.status === "canceled") return qsTr("已取消")
                                return ""
                            }
                            font.pixelSize: ThemeProvider.fontSizeSmall
                            color: {
                                if (item.status === "completed") return ThemeProvider.successColor
                                if (item.status === "failed") return ThemeProvider.dangerColor
                                if (item.status === "canceled") return ThemeProvider.textSecondary
                                if (item.status === "paused") return ThemeProvider.warningColor
                                return ThemeProvider.textSecondary
                            }
                        }
                    }
                    
                    ProgressIndicator {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 4
                        showText: false
                        flat: true
                        indeterminate: delegateRoot.indeterminate
                        value: (item.progress >= 0) ? item.progress : 0
                        barColor: {
                            if (item.status === "completed") return ThemeProvider.successColor
                            if (item.status === "failed") return ThemeProvider.dangerColor
                            if (item.status === "paused") return ThemeProvider.warningColor
                            return ThemeProvider.primaryColor
                        }
                    }
                    
                    // 操作按钮
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: ThemeProvider.spacingSmall
                        
                        // 进度文本：大小 + 百分比 + 速度
                        Text {
                            text: {
                                var status = item.status
                                if (status === "downloading" || status === "paused") {
                                    var received = item.bytesReceived || 0
                                    var total = item.bytesTotal || 0
                                    var result = formatFileSize(received)
                                    if (total > 0) {
                                        var pct = Math.round((item.progress || 0) * 100)
                                        result += " / " + formatFileSize(total) + "  " + pct + "%"
                                    }
                                    if (status === "downloading") {
                                        var spd = item.speed || 0
                                        if (spd > 0) {
                                            result += "  " + formatSpeed(spd)
                                        }
                                    }
                                    return result
                                }
                                if (status === "completed") {
                                    var fileSize = item.bytesTotal || item.bytesReceived || 0
                                    if (fileSize > 0) {
                                        return formatFileSize(fileSize)
                                    }
                                }
                                if (status === "failed" && item.errorMessage) {
                                    return item.errorMessage
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
                            visible: item.status === "downloading" || item.status === "paused" || item.status === "queued"
                            // 排队中的任务只能暂停：后端的 resumeDownload 只接受
                            // paused / failed，所以这里原本显示的「继续」点了没有
                            // 任何反应。pauseDownload 本来就支持 queued。
                            iconSource: item.status === "paused"
                                        ? ThemeProvider.assetUrl("icons/step-forward.png")
                                        : ThemeProvider.assetUrl("icons/pause.png")
                            iconSize: 12
                            tooltip: item.status === "paused" ? qsTr("继续") : qsTr("暂停")
                            onClicked: {
                                if (item.status === "paused") {
                                    resumeDownload(index)
                                } else {
                                    pauseDownload(index)
                                }
                            }
                        }
                        
                        // 重试按钮（失败任务）
                        IconButton {
                            visible: item.status === "failed"
                            iconSource: ThemeProvider.assetUrl("icons/step-forward.png")
                            iconSize: 12
                            tooltip: qsTr("重试")
                            onClicked: resumeDownload(index)
                        }
                        
                        // 取消按钮
                        IconButton {
                            visible: item.status === "downloading" || item.status === "paused" || item.status === "queued"
                            iconSource: ThemeProvider.assetUrl("icons/exit.png")
                            iconSize: 12
                            tooltip: qsTr("取消")
                            onClicked: cancelDownload(index)
                        }
                        
                        // 打开文件夹按钮
                        IconButton {
                            visible: item.status === "completed"
                            iconSource: ThemeProvider.assetUrl("icons/folder.png")
                            iconSize: 12
                            tooltip: qsTr("打开文件夹")
                            onClicked: openFolder(index)
                        }
                        
                        // 删除条目按钮
                        IconButton {
                            visible: item.status === "completed" || item.status === "failed" || item.status === "canceled"
                            iconSource: ThemeProvider.assetUrl("icons/delete.png")
                            iconSize: 12
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
                color: ThemeProvider.textMuted
            }
        }
    }
}
