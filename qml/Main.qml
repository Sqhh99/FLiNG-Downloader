import QtQuick
import QtQuick.Window
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

import FLiNGDownloader

/**
 * Main.qml - 主窗口
 * 无边框设计，自定义标题栏
 */
ApplicationWindow {
    id: mainWindow
    
    width: 1200
    height: 800
    minimumWidth: 800
    minimumHeight: 600
    visible: true
    title: qsTr("FLiNG Downloader")
    
    // 无边框窗口
    flags: Qt.FramelessWindowHint | Qt.Window
    
    color: ThemeProvider.backgroundColor
    
    // backend 由 main.cpp 通过 rootContext 注入
    required property var backend
    
    // 待下载标志 - 用于等待详情加载完成后自动开始下载
    property bool pendingDownload: false
    property int pendingDownloadIndex: -1
    
    Component.onCompleted: {
        console.log("Main.qml 加载完成")
        console.log("Backend 对象:", backend)
        console.log("ModifierListModel:", backend ? backend.modifierListModel : "null")
        if (backend && backend.modifierListModel) {
            console.log("模型行数:", backend.modifierListModel.rowCount())
        }
    }
    
    // 主内容区
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // 自定义标题栏
        CustomTitleBar {
            id: customTitleBar
            Layout.fillWidth: true
            title: mainWindow.title
            targetWindow: mainWindow
            activeDownloads: downloadListPopup.activeDownloads
            
            onDownloadClicked: {
                // 切换下载列表显示
                console.log("下载按钮点击, popup.visible:", downloadListPopup.visible)
                if (downloadListPopup.visible) {
                    console.log("关闭下载列表")
                    downloadListPopup.close()
                } else {
                    console.log("打开下载列表")
                    downloadListPopup.open()
                }
            }
            
            onSettingsClicked: {
                settingsDialog.currentTheme = ThemeProvider.currentTheme
                settingsDialog.currentLanguage = backend ? backend.currentLanguage : 0
                // 切换设置对话框显示
                if (settingsDialog.visible) {
                    settingsDialog.close()
                } else {
                    settingsDialog.open()
                }
            }
        }
        
        // 标签页
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            
            background: Rectangle {
                color: ThemeProvider.backgroundColor
                
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: ThemeProvider.borderColor
                }
            }
            
            TabButton {
                text: qsTr("搜索修改器")
                width: implicitWidth + 40
                
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: ThemeProvider.fontSizeMedium
                    font.bold: parent.checked
                    color: parent.checked ? ThemeProvider.primaryColor : ThemeProvider.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                background: Rectangle {
                    color: parent.checked ? ThemeProvider.surfaceColor : "transparent"
                    
                    Rectangle {
                        visible: parent.parent.checked
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 2
                        color: ThemeProvider.primaryColor
                    }
                }
            }
            
            TabButton {
                text: qsTr("已下载修改器")
                width: implicitWidth + 40
                
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: ThemeProvider.fontSizeMedium
                    font.bold: parent.checked
                    color: parent.checked ? ThemeProvider.primaryColor : ThemeProvider.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                background: Rectangle {
                    color: parent.checked ? ThemeProvider.surfaceColor : "transparent"
                    
                    Rectangle {
                        visible: parent.parent.checked
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 2
                        color: ThemeProvider.primaryColor
                    }
                }
            }
        }
        
        // 内容区
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex
            
            // 搜索页 - 不再使用 SplitView
            SearchPage {
                id: searchPage
                backend: mainWindow.backend  // 传入后端对象
                modifierModel: backend ? backend.modifierListModel : null
                
                onSearchRequested: function(keyword) {
                    if (backend) backend.searchModifiers(keyword)
                }
                
                onModifierSelected: function(index) {
                    // 行选择由 SearchPage 内部处理
                    console.log("行点击, index:", index)
                }
                
                onSortChanged: function(sortIndex) {
                    if (backend) backend.setSortOrder(sortIndex)
                }
                
                onRefreshRequested: {
                    if (backend) backend.fetchRecentModifiers()
                }
                
                // 详情按钮 - 打开详情面板，并选择对应行
                onDetailsRequested: function(index) {
                    // 选择对应的行
                    searchPage.selectRow(index)
                    
                    if (backend) backend.selectModifier(index)
                    
                    // 切换详情面板显示
                    if (detailDrawer.visible) {
                        detailDrawer.close()
                    } else {
                        detailDrawer.open()
                    }
                }
                
                // 下载功能已移至详情面板
            }
            
            // 已下载页
            DownloadedPage {
                id: downloadedPage
                downloadedModel: backend ? backend.downloadedModifierModel : null
                
                onOpenFolderRequested: function(index) {
                    if (backend) backend.openDownloadFolder()  // 打开下载文件夹
                }
                
                onDeleteModifier: function(index) {
                    if (backend) backend.deleteModifier(index)
                }
                
                onCheckUpdates: {
                    if (backend) backend.checkForUpdates()
                    statusBar.showMessage(qsTr("正在检查更新..."), 0)
                }
                
                onModifierDoubleClicked: function(index) {
                    if (backend) backend.runModifier(index)
                }
            }
        }
    }
    
    // 右侧弹出详情面板
    DetailDrawer {
        id: detailDrawer
        parent: Overlay.overlay
        
        gameName: backend ? backend.selectedModifierName : ""
        gameVersion: backend ? backend.selectedModifierVersion : ""
        optionsCount: backend ? backend.selectedModifierOptionsCount : 0
        lastUpdate: backend ? backend.selectedModifierLastUpdate : ""
        optionsHtml: backend ? backend.selectedModifierOptions : ""
        versions: backend ? backend.selectedModifierVersions : []
        coverUrl: backend ? backend.selectedModifierCoverPath : ""
        
        onVersionChanged: function(index) {
            if (backend) backend.selectVersion(index)
        }
        
        // 从详情面板触发下载
        onStartDownload: function(versionIndex) {
            console.log("从详情面板下载, 修改器:", gameName, "版本索引:", versionIndex)
            
            if (backend) {
                // 添加到下载列表
                var newItem = {
                    fileName: gameName,
                    progress: 0,
                    status: "downloading",
                    downloadIndex: versionIndex
                }
                var items = downloadListPopup.downloadItems.slice()
                items.push(newItem)
                downloadListPopup.downloadItems = items
                downloadListPopup.activeDownloads = downloadListPopup.downloadItems.filter(function(item) { 
                    return item.status === "downloading" 
                }).length
                
                // 开始下载
                backend.downloadModifier(versionIndex)
                downloadListPopup.open()
            }
        }
    }
    
    // 后端连接
    Connections {
        target: backend
        
        function onSearchCompleted() {
            // 搜索完成
        }
        
        // 当修改器详情加载完成时，检查是否有待下载任务
        function onSelectedModifierChanged() {
            console.log("详情加载完成, pendingDownload:", mainWindow.pendingDownload, "版本数:", backend ? backend.selectedModifierVersions.length : 0)
            
            // 只有当有待下载任务 AND 版本列表不为空时才开始下载
            if (mainWindow.pendingDownload && backend && backend.selectedModifierVersions.length > 0) {
                mainWindow.pendingDownload = false
                console.log("开始真正下载, 版本:", backend.selectedModifierVersions[0])
                
                // 开始真正的下载
                backend.downloadModifier(0)  // 下载第一个版本
            }
            // 如果版本列表为空，保持 pendingDownload 为 true，等待下一次信号
        }
        
        function onDownloadCompleted(success) {
            // 下载完成通知已由下载列表处理
        }
        
        function onStatusMessage(message) {
            // 状态消息已移除
        }
    }
    
    // 下载列表弹窗
    DownloadListPopup {
        id: downloadListPopup
        parent: Overlay.overlay
        
        // 下载列表数据
        downloadItems: []
        activeDownloads: 0
        
        onOpenFolder: function(index) {
            if (backend) backend.openDownloadFolder()
        }
        
        onRemoveFromList: function(index) {
            var items = downloadItems.slice()
            items.splice(index, 1)
            downloadItems = items
        }
    }
    
    // 后端下载进度连接
    Connections {
        target: backend
        
        function onDownloadProgressChanged() {
            // 更新最后一个下载任务的进度（从backend属性获取进度值）
            if (downloadListPopup.downloadItems.length > 0 && backend) {
                var progress = backend.downloadProgress * 100  // 转换为百分比
                var items = downloadListPopup.downloadItems.slice()
                for (var i = items.length - 1; i >= 0; i--) {
                    if (items[i].status === "downloading") {
                        items[i].progress = progress
                        break
                    }
                }
                downloadListPopup.downloadItems = items
            }
        }
        
        function onDownloadCompleted(success) {
            // 更新最后一个下载任务的状态
            if (downloadListPopup.downloadItems.length > 0) {
                var items = downloadListPopup.downloadItems.slice()
                for (var i = items.length - 1; i >= 0; i--) {
                    if (items[i].status === "downloading") {
                        items[i].status = success ? "completed" : "failed"
                        items[i].progress = success ? 100 : items[i].progress
                        break
                    }
                }
                downloadListPopup.downloadItems = items
                downloadListPopup.activeDownloads = items.filter(function(item) { 
                    return item.status === "downloading" 
                }).length
            }
            // 下载完成通知已由列表处理
        }
    }
    
    // 设置对话框
    SettingsDialog {
        id: settingsDialog
        
        // 绑定下载路径
        downloadPath: backend ? backend.downloadPath : ""
        
        onThemeChanged: function(index) {
            ThemeProvider.currentTheme = index
            if (backend) backend.setTheme(index)
        }
        
        onLanguageChangedSignal: function(index) {
            console.log("Main: 语言切换信号收到, index:", index)
            if (backend) backend.setLanguage(index)
        }
        
        onBrowseDownloadPath: {
            console.log("Main: 打开下载目录选择对话框")
            downloadFolderDialog.open()
        }
    }
    
    // 下载目录选择对话框
    FolderDialog {
        id: downloadFolderDialog
        title: qsTr("选择下载目录")
        currentFolder: backend ? "file:///" + backend.downloadPath : ""
        onAccepted: {
            if (backend) {
                var path = selectedFolder.toString().replace("file:///", "")
                backend.setDownloadPath(path)
                console.log("已选择下载目录:", path)
            }
        }
    }
    
    // 响应 Backend 信号请求打开文件夹选择对话框
    Connections {
        target: backend
        function onDownloadFolderSelectionRequested() {
            downloadFolderDialog.open()
        }
    }
    
    // 窗口调整大小边框
    MouseArea {
        id: resizeArea
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 16
        height: 16
        cursorShape: Qt.SizeFDiagCursor
        
        property point clickPos: Qt.point(0, 0)
        
        onPressed: function(mouse) {
            clickPos = Qt.point(mouse.x, mouse.y)
        }
        
        onPositionChanged: function(mouse) {
            if (pressed) {
                var newWidth = mainWindow.width + (mouse.x - clickPos.x)
                var newHeight = mainWindow.height + (mouse.y - clickPos.y)
                mainWindow.width = Math.max(mainWindow.minimumWidth, newWidth)
                mainWindow.height = Math.max(mainWindow.minimumHeight, newHeight)
            }
        }
        
        // 调整大小指示器
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            
            Canvas {
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.strokeStyle = ThemeProvider.borderColor
                    ctx.lineWidth = 1
                    for (var i = 0; i < 3; i++) {
                        ctx.beginPath()
                        ctx.moveTo(width - 3 - i * 4, height)
                        ctx.lineTo(width, height - 3 - i * 4)
                        ctx.stroke()
                    }
                }
            }
        }
    }
}
