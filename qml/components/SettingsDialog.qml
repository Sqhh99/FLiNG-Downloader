import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FLiNGDownloader

/**
 * SettingsDialog - 设置对话框
 * 侧边栏导航布局，设置主题、语言、下载路径
 */
Dialog {
    id: settingsDialog
    
    property int currentTheme: ThemeProvider.currentTheme
    property int currentLanguage: 0
    property string downloadPath: ""
    property string appVersion: ""
    property bool autoCheckUpdates: true
    property int updateSource: 0  // 0 = GitHub, 1 = Gitee
    // Display name that follows the selector, so the "源" label updates immediately.
    readonly property string updateSourceName: updateSource === 1 ? "Gitee" : "GitHub"
    property bool appUpdateChecking: false
    property bool appUpdateAvailable: false
    property string appLatestVersion: ""
    property string appUpdateSource: ""
    property string appUpdatePublishedAt: ""
    property bool appUpdateDownloading: false
    property real appUpdateProgress: 0
    property string appUpdateStatusText: ""
    property bool autoCheckDatabaseUpdates: true
    property string databaseCurrentVersion: ""
    property bool databaseUpdateChecking: false
    property bool databaseUpdateAvailable: false
    property string databaseLatestVersion: ""
    property string databaseUpdateSource: ""
    property string databaseUpdatePublishedAt: ""
    property bool databaseUpdateDownloading: false
    property real databaseUpdateProgress: 0
    property string databaseUpdateStatusText: ""
    
    signal themeChanged(int index)
    signal languageChangedSignal(int index)  // 重命名避免冲突
    signal browseDownloadPath()
    signal autoCheckUpdatesToggled(bool enabled)
    signal autoCheckDatabaseUpdatesToggled(bool enabled)
    signal updateSourceSelected(int index)
    signal checkAppUpdateRequested()
    signal downloadAppUpdateRequested()
    signal checkDatabaseUpdateRequested()
    signal downloadDatabaseUpdateRequested()
    signal settingsApplied()
    
    title: qsTr("设置")
    modal: true
    anchors.centerIn: parent
    width: 600
    height: 560
    
    // 自定义背景
    background: Rectangle {
        color: ThemeProvider.surfaceColor
        radius: ThemeProvider.radiusMedium
        border.color: ThemeProvider.borderColor
    }
    
    // 自定义头部
    header: Rectangle {
        height: 50
        color: ThemeProvider.surfaceColor
        radius: ThemeProvider.radiusMedium
        
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: ThemeProvider.spacingMedium
            anchors.rightMargin: ThemeProvider.spacingMedium
            
            Text {
                text: qsTr("设置")
                font.pixelSize: ThemeProvider.fontSizeTitle
                font.bold: true
                color: ThemeProvider.textPrimary
                Layout.fillWidth: true
            }
            
            // 关闭按钮
            Rectangle {
                width: 30
                height: 30
                radius: ThemeProvider.radiusSmall
                color: closeArea.containsMouse ? ThemeProvider.errorColor : "transparent"
                
                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    font.pixelSize: 16
                    color: closeArea.containsMouse ? "#fff" : ThemeProvider.textSecondary
                }
                
                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settingsDialog.close()
                }
            }
        }
        
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: ThemeProvider.borderColor
        }
    }
    
    contentItem: RowLayout {
        spacing: 0
        
        // 侧边栏
        Rectangle {
            Layout.preferredWidth: 140
            Layout.fillHeight: true
            color: ThemeProvider.backgroundColor
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: ThemeProvider.spacingSmall
                spacing: 4
                
                // 外观设置
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    radius: ThemeProvider.radiusSmall
                    color: settingsStack.currentIndex === 0 ? ThemeProvider.selectedColor : "transparent"
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        spacing: 8
                        
                        Image {
                            source: ThemeProvider.assetUrl("icons/settings.png")
                            width: 18
                            height: 18
                            sourceSize: Qt.size(18, 18)
                        }
                        
                        Text {
                            text: qsTr("外观")
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textPrimary
                            Layout.fillWidth: true
                        }
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsStack.currentIndex = 0
                    }
                }
                
                // 下载设置
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    radius: ThemeProvider.radiusSmall
                    color: settingsStack.currentIndex === 1 ? ThemeProvider.selectedColor : "transparent"
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        spacing: 8
                        
                        Image {
                            source: ThemeProvider.assetUrl("icons/download.png")
                            width: 18
                            height: 18
                            sourceSize: Qt.size(18, 18)
                        }
                        
                        Text {
                            text: qsTr("下载")
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textPrimary
                            Layout.fillWidth: true
                        }
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsStack.currentIndex = 1
                    }
                }
                
                // 语言设置
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    radius: ThemeProvider.radiusSmall
                    color: settingsStack.currentIndex === 2 ? ThemeProvider.selectedColor : "transparent"
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        spacing: 8
                        
                        Image {
                            source: ThemeProvider.assetUrl("icons/language.png")
                            width: 18
                            height: 18
                            sourceSize: Qt.size(18, 18)
                        }
                        
                        Text {
                            text: qsTr("语言")
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textPrimary
                            Layout.fillWidth: true
                        }
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsStack.currentIndex = 2
                    }
                }
                
                // 关于
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    radius: ThemeProvider.radiusSmall
                    color: settingsStack.currentIndex === 3 ? ThemeProvider.selectedColor : "transparent"
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        spacing: 8
                        
                        Image {
                            source: ThemeProvider.assetUrl("icons/about.png")
                            width: 18
                            height: 18
                            sourceSize: Qt.size(18, 18)
                        }
                        
                        Text {
                            text: qsTr("关于")
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textPrimary
                            Layout.fillWidth: true
                        }
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsStack.currentIndex = 3
                    }
                }
                
                Item { Layout.fillHeight: true }
            }
        }
        
        // 内容区
        StackLayout {
            id: settingsStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: 0
            
            // 外观设置页
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: ThemeProvider.spacingMedium
                    spacing: ThemeProvider.spacingMedium
                    
                    Text {
                        text: qsTr("外观设置")
                        font.pixelSize: ThemeProvider.fontSizeTitle
                        font.bold: true
                        color: ThemeProvider.textPrimary
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: ThemeProvider.borderColor
                    }
                    
                    // 主题选择
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: ThemeProvider.spacingSmall
                        
                        Text {
                            text: qsTr("主题")
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textSecondary
                        }
                        
                        // 使用 GridLayout 显示所有主题
                        GridLayout {
                            columns: 5
                            rowSpacing: ThemeProvider.spacingSmall
                            columnSpacing: ThemeProvider.spacingSmall
                            
                            Repeater {
                                model: [
                                    {name: qsTr("浅色"), bgColor: "#FAFAFA", textColor: "#333"},
                                    {name: qsTr("深色"), bgColor: "#1A1D23", textColor: "#fff"},
                                    {name: qsTr("海洋"), bgColor: "#E0F7FA", textColor: "#006064"},
                                    {name: qsTr("日落"), bgColor: "#FFF8E1", textColor: "#4E342E"},
                                    {name: qsTr("森林"), bgColor: "#E8F5E9", textColor: "#1B5E20"},
                                    {name: qsTr("薰衣草"), bgColor: "#F3E5F5", textColor: "#4A148C"},
                                    {name: qsTr("玫瑰"), bgColor: "#FCE4EC", textColor: "#880E4F"},
                                    {name: qsTr("午夜"), bgColor: "#1A1A2E", textColor: "#E8EAF6"},
                                    {name: qsTr("摩卡"), bgColor: "#EFEBE9", textColor: "#3E2723"}
                                ]
                                
                                delegate: Rectangle {
                                    width: 72
                                    height: 50
                                    radius: ThemeProvider.radiusSmall
                                    color: modelData.bgColor
                                    border.width: currentTheme === index ? 2 : 1
                                    border.color: currentTheme === index ? ThemeProvider.primaryColor : ThemeProvider.borderColor
                                    
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.name
                                        font.pixelSize: ThemeProvider.fontSizeSmall
                                        color: modelData.textColor
                                    }
                                    
                                    // 选中标记
                                    Rectangle {
                                        visible: currentTheme === index
                                        anchors.top: parent.top
                                        anchors.right: parent.right
                                        anchors.margins: 4
                                        width: 12
                                        height: 12
                                        radius: 6
                                        color: ThemeProvider.primaryColor
                                        
                                        Text {
                                            anchors.centerIn: parent
                                            text: "✓"
                                            font.pixelSize: 8
                                            font.bold: true
                                            color: "#fff"
                                        }
                                    }
                                    
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            currentTheme = index
                                            themeChanged(index)
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    Item { Layout.fillHeight: true }
                }
            }
            
            // 下载设置页
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: ThemeProvider.spacingMedium
                    spacing: ThemeProvider.spacingMedium
                    
                    Text {
                        text: qsTr("下载设置")
                        font.pixelSize: ThemeProvider.fontSizeTitle
                        font.bold: true
                        color: ThemeProvider.textPrimary
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: ThemeProvider.borderColor
                    }
                    
                    // 下载目录设置
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: ThemeProvider.spacingSmall
                        
                        Text {
                            text: qsTr("下载目录")
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textSecondary
                        }
                        
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: ThemeProvider.spacingSmall
                            
                            Rectangle {
                                Layout.fillWidth: true
                                height: 36
                                radius: ThemeProvider.radiusSmall
                                color: ThemeProvider.inputBackground
                                border.color: ThemeProvider.borderColor
                                
                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    text: settingsDialog.downloadPath || qsTr("未设置")
                                    font.pixelSize: ThemeProvider.fontSizeMedium
                                    color: ThemeProvider.textPrimary
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideMiddle
                                }
                            }
                            
                            StyledButton {
                                text: qsTr("浏览...")
                                buttonType: "secondary"
                                onClicked: {
                                    browseDownloadPath()
                                }
                            }
                        }
                        
                        Text {
                            text: qsTr("修改器将下载到此目录")
                            font.pixelSize: ThemeProvider.fontSizeSmall
                            color: ThemeProvider.textDisabled
                        }
                    }
                    
                    Item { Layout.fillHeight: true }
                }
            }
            
            // 语言设置页
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: ThemeProvider.spacingMedium
                    spacing: ThemeProvider.spacingMedium
                    
                    Text {
                        text: qsTr("语言设置")
                        font.pixelSize: ThemeProvider.fontSizeTitle
                        font.bold: true
                        color: ThemeProvider.textPrimary
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: ThemeProvider.borderColor
                    }
                    
                    // 语言选择
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: ThemeProvider.spacingSmall
                        
                        Text {
                            text: qsTr("界面语言")
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textSecondary
                        }
                        
                        StyledComboBox {
                            id: languageComboBox
                            Layout.preferredWidth: 200
                            model: [qsTr("简体中文"), qsTr("English"), qsTr("日本語")]
                            currentIndex: settingsDialog.currentLanguage
                            
                            onActivated: function(index) {
                                console.log("语言切换:", index)
                                settingsDialog.currentLanguage = index
                                settingsDialog.languageChangedSignal(index)
                            }
                        }
                        
                        Text {
                            text: qsTr("切换语言后需要重启应用才能完全生效")
                            font.pixelSize: ThemeProvider.fontSizeSmall
                            color: ThemeProvider.textDisabled
                        }
                    }
                    
                    Item { Layout.fillHeight: true }
                }
            }
            
            // 关于页 - 使用 Flickable 实现滚动，紧凑卡片式布局
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: ThemeProvider.spacingMedium
                    spacing: ThemeProvider.spacingSmall

                    Text {
                        text: qsTr("关于")
                        font.pixelSize: ThemeProvider.fontSizeTitle
                        font.bold: true
                        color: ThemeProvider.textPrimary
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: ThemeProvider.borderColor
                    }

                    // 可滚动内容区
                    Flickable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: width
                        contentHeight: aboutContentColumn.implicitHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        flickableDirection: Flickable.VerticalFlick

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            width: 4

                            contentItem: Rectangle {
                                implicitWidth: 4
                                radius: 2
                                color: ThemeProvider.textDisabled
                                opacity: 0.5
                            }
                        }

                        ColumnLayout {
                            id: aboutContentColumn
                            // Leave room for the overlay scrollbar so the cards and
                            // download progress bar are not clipped at the right edge.
                            width: parent.width - 12
                            spacing: ThemeProvider.spacingSmall

                            // ── 应用信息（紧凑单行） ──
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: ThemeProvider.spacingMedium

                                Text {
                                    text: "FLiNG Downloader"
                                    font.pixelSize: ThemeProvider.fontSizeMedium
                                    font.bold: true
                                    color: ThemeProvider.textPrimary
                                }

                                Text {
                                    text: "v" + (settingsDialog.appVersion.length > 0 ? settingsDialog.appVersion : "0.0.0-dev")
                                    font.pixelSize: ThemeProvider.fontSizeSmall
                                    color: ThemeProvider.textSecondary
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    text: qsTr("作者: Sqhh99")
                                    font.pixelSize: ThemeProvider.fontSizeSmall
                                    color: ThemeProvider.textSecondary
                                }
                            }

                            // 开源地址
                            Item {
                                Layout.fillWidth: true
                                height: 20

                                Text {
                                    anchors.fill: parent
                                    text: "GitHub: https://github.com/Sqhh99/FLiNG-Downloader"
                                    font.pixelSize: ThemeProvider.fontSizeSmall
                                    color: ThemeProvider.primaryColor
                                    font.underline: true
                                    elide: Text.ElideMiddle
                                    verticalAlignment: Text.AlignVCenter
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Qt.openUrlExternally("https://github.com/Sqhh99/FLiNG-Downloader")
                                }
                            }

                            // ── 更新源选择（软件与数据库更新共用）──
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: ThemeProvider.spacingMedium

                                Text {
                                    text: qsTr("更新源")
                                    font.pixelSize: ThemeProvider.fontSizeMedium
                                    color: ThemeProvider.textSecondary
                                }

                                StyledComboBox {
                                    id: updateSourceComboBox
                                    Layout.preferredWidth: 160
                                    model: ["GitHub", "Gitee"]
                                    currentIndex: settingsDialog.updateSource
                                    onActivated: function(index) {
                                        settingsDialog.updateSource = index
                                        settingsDialog.updateSourceSelected(index)
                                    }
                                }

                                // Flexible + elide so the hint can shrink instead of
                                // forcing the whole about column wider than the dialog.
                                Text {
                                    text: qsTr("软件与数据库更新共用此源")
                                    font.pixelSize: ThemeProvider.fontSizeSmall
                                    color: ThemeProvider.textDisabled
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }

                            // ── 软件更新卡片 ──
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: appUpdateContent.implicitHeight + 20
                                radius: ThemeProvider.radiusSmall
                                color: ThemeProvider.backgroundColor
                                border.color: ThemeProvider.borderColor

                                ColumnLayout {
                                    id: appUpdateContent
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 6

                                    // 标题行 + 自动检查开关
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: ThemeProvider.spacingSmall

                                        Text {
                                            text: qsTr("软件更新")
                                            font.pixelSize: ThemeProvider.fontSizeMedium
                                            font.bold: true
                                            color: ThemeProvider.textPrimary
                                        }

                                        Item { Layout.fillWidth: true }

                                        StyledSwitch {
                                            id: autoUpdateSwitch
                                            checked: settingsDialog.autoCheckUpdates
                                            onToggled: settingsDialog.autoCheckUpdatesToggled(checked)
                                        }

                                        Text {
                                            text: qsTr("自动检查")
                                            font.pixelSize: ThemeProvider.fontSizeSmall
                                            color: ThemeProvider.textSecondary
                                        }
                                    }

                                    // 状态文本
                                    Text {
                                        text: settingsDialog.appUpdateStatusText.length > 0
                                            ? settingsDialog.appUpdateStatusText
                                            : qsTr("点击“检查更新”获取最新版本信息")
                                        font.pixelSize: ThemeProvider.fontSizeSmall
                                        color: ThemeProvider.textSecondary
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    // 更新详情（合并为一行，仅在有信息时显示）
                                    RowLayout {
                                        visible: settingsDialog.appLatestVersion.length > 0
                                        spacing: ThemeProvider.spacingMedium
                                        Layout.fillWidth: true

                                        Text {
                                            visible: settingsDialog.appLatestVersion.length > 0
                                            text: qsTr("最新: %1").arg(settingsDialog.appLatestVersion)
                                            font.pixelSize: ThemeProvider.fontSizeSmall
                                            color: ThemeProvider.textSecondary
                                        }

                                        Text {
                                            visible: settingsDialog.appLatestVersion.length > 0
                                            text: qsTr("源: %1").arg(settingsDialog.updateSourceName)
                                            font.pixelSize: ThemeProvider.fontSizeSmall
                                            color: ThemeProvider.textSecondary
                                        }

                                        Text {
                                            visible: settingsDialog.appUpdatePublishedAt.length > 0
                                            text: qsTr("发布: %1").arg(settingsDialog.appUpdatePublishedAt)
                                            font.pixelSize: ThemeProvider.fontSizeSmall
                                            color: ThemeProvider.textSecondary
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }

                                    // 下载进度
                                    ProgressIndicator {
                                        visible: settingsDialog.appUpdateDownloading
                                        Layout.fillWidth: true
                                        showText: true
                                        statusText: Math.round(Math.max(0, Math.min(1, settingsDialog.appUpdateProgress)) * 100) + "%"
                                        value: Math.max(0, Math.min(1, settingsDialog.appUpdateProgress))
                                    }

                                    // 操作按钮
                                    RowLayout {
                                        spacing: ThemeProvider.spacingSmall

                                        StyledButton {
                                            text: settingsDialog.appUpdateChecking
                                                ? qsTr("检查中...")
                                                : qsTr("检查更新")
                                            buttonType: "secondary"
                                            enabled: !settingsDialog.appUpdateChecking && !settingsDialog.appUpdateDownloading
                                            onClicked: settingsDialog.checkAppUpdateRequested()
                                        }

                                        StyledButton {
                                            visible: settingsDialog.appUpdateAvailable
                                            text: settingsDialog.appUpdateDownloading
                                                ? qsTr("下载中...")
                                                : qsTr("下载并安装")
                                            enabled: settingsDialog.appUpdateAvailable
                                                && !settingsDialog.appUpdateChecking
                                                && !settingsDialog.appUpdateDownloading
                                            onClicked: settingsDialog.downloadAppUpdateRequested()
                                        }
                                    }
                                }
                            }

                            // ── 翻译数据库更新卡片 ──
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: dbUpdateContent.implicitHeight + 20
                                radius: ThemeProvider.radiusSmall
                                color: ThemeProvider.backgroundColor
                                border.color: ThemeProvider.borderColor

                                ColumnLayout {
                                    id: dbUpdateContent
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 6

                                    // 标题行 + 自动检查开关
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: ThemeProvider.spacingSmall

                                        Text {
                                            text: qsTr("翻译数据库更新")
                                            font.pixelSize: ThemeProvider.fontSizeMedium
                                            font.bold: true
                                            color: ThemeProvider.textPrimary
                                        }

                                        Item { Layout.fillWidth: true }

                                        StyledSwitch {
                                            checked: settingsDialog.autoCheckDatabaseUpdates
                                            onToggled: settingsDialog.autoCheckDatabaseUpdatesToggled(checked)
                                        }

                                        Text {
                                            text: qsTr("自动检查")
                                            font.pixelSize: ThemeProvider.fontSizeSmall
                                            color: ThemeProvider.textSecondary
                                        }
                                    }

                                    // 当前版本
                                    Text {
                                        text: qsTr("当前版本: %1").arg(
                                                  settingsDialog.databaseCurrentVersion.length > 0
                                                  ? settingsDialog.databaseCurrentVersion
                                                  : qsTr("未知"))
                                        font.pixelSize: ThemeProvider.fontSizeSmall
                                        color: ThemeProvider.textSecondary
                                    }

                                    // 状态文本
                                    Text {
                                        text: settingsDialog.databaseUpdateStatusText.length > 0
                                            ? settingsDialog.databaseUpdateStatusText
                                            : qsTr("点击“检查更新”获取最新数据库版本信息")
                                        font.pixelSize: ThemeProvider.fontSizeSmall
                                        color: ThemeProvider.textSecondary
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    // 更新详情（合并为一行）
                                    RowLayout {
                                        visible: settingsDialog.databaseLatestVersion.length > 0
                                        spacing: ThemeProvider.spacingMedium
                                        Layout.fillWidth: true

                                        Text {
                                            visible: settingsDialog.databaseLatestVersion.length > 0
                                            text: qsTr("最新: %1").arg(settingsDialog.databaseLatestVersion)
                                            font.pixelSize: ThemeProvider.fontSizeSmall
                                            color: ThemeProvider.textSecondary
                                        }

                                        Text {
                                            visible: settingsDialog.databaseLatestVersion.length > 0
                                            text: qsTr("源: %1").arg(settingsDialog.updateSourceName)
                                            font.pixelSize: ThemeProvider.fontSizeSmall
                                            color: ThemeProvider.textSecondary
                                        }

                                        Text {
                                            visible: settingsDialog.databaseUpdatePublishedAt.length > 0
                                            text: qsTr("发布: %1").arg(settingsDialog.databaseUpdatePublishedAt)
                                            font.pixelSize: ThemeProvider.fontSizeSmall
                                            color: ThemeProvider.textSecondary
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }

                                    // 下载进度
                                    ProgressIndicator {
                                        visible: settingsDialog.databaseUpdateDownloading
                                        Layout.fillWidth: true
                                        showText: true
                                        statusText: Math.round(Math.max(0, Math.min(1, settingsDialog.databaseUpdateProgress)) * 100) + "%"
                                        value: Math.max(0, Math.min(1, settingsDialog.databaseUpdateProgress))
                                    }

                                    // 操作按钮
                                    RowLayout {
                                        spacing: ThemeProvider.spacingSmall

                                        StyledButton {
                                            text: settingsDialog.databaseUpdateChecking
                                                ? qsTr("检查中...")
                                                : qsTr("检查数据库更新")
                                            buttonType: "secondary"
                                            enabled: !settingsDialog.databaseUpdateChecking && !settingsDialog.databaseUpdateDownloading
                                            onClicked: settingsDialog.checkDatabaseUpdateRequested()
                                        }

                                        StyledButton {
                                            visible: settingsDialog.databaseUpdateAvailable
                                            text: settingsDialog.databaseUpdateDownloading
                                                ? qsTr("下载中...")
                                                : qsTr("下载并应用")
                                            enabled: settingsDialog.databaseUpdateAvailable
                                                && !settingsDialog.databaseUpdateChecking
                                                && !settingsDialog.databaseUpdateDownloading
                                            onClicked: settingsDialog.downloadDatabaseUpdateRequested()
                                        }
                                    }
                                }
                            }

                            // 底部留白
                            Item { height: 4; Layout.fillWidth: true }
                        }
                    }
                }
            }
        }
    }
    
    footer: Rectangle {
        height: 50
        color: ThemeProvider.surfaceColor
        radius: ThemeProvider.radiusMedium
        
        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: ThemeProvider.borderColor
        }
    }
}
