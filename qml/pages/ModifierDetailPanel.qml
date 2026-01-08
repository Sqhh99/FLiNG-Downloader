import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import "../components"
import "../themes"

/**
 * ModifierDetailPanel - 修改器详情面板
 * 显示选中修改器的详细信息、版本选择和下载功能
 */
Item {
    id: detailPanel
    
    // 属性
    property string gameName: ""
    property string gameVersion: ""
    property int optionsCount: 0
    property string lastUpdate: ""
    property string optionsHtml: ""
    property var versions: []  // [{name: "v1.0", url: "..."}]
    property int selectedVersionIndex: 0
    property real downloadProgress: 0
    property bool downloading: false
    property string coverUrl: ""
    
    // 信号
    signal downloadRequested(int versionIndex)
    signal openFolderRequested()
    signal settingsRequested()
    signal versionChanged(int index)
    
    visible: gameName.length > 0
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: ThemeProvider.spacingSmall
        spacing: 0
        
        // 游戏标题
        Text {
            text: gameName
            font.pixelSize: ThemeProvider.fontSizeTitle
            font.bold: true
            color: ThemeProvider.textPrimary
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        
        // 封面和信息区
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 160
            spacing: ThemeProvider.spacingMedium
            
            // 游戏封面
            Rectangle {
                Layout.preferredWidth: 120
                Layout.preferredHeight: 160
                radius: ThemeProvider.radiusMedium
                color: "transparent" // 透明背景，避免显示边框
                // 移除边框，防止显示空白区域
                
                Image {
                    id: coverImage
                    anchors.fill: parent
                    anchors.margins: 0 // 移除边距
                    source: coverUrl
                    fillMode: Image.PreserveAspectFit
                    mipmap: true // 开启mipmap抗锯齿
                    
                    // 加载中提示
                    Text {
                        visible: coverImage.status === Image.Loading || coverUrl.length === 0
                        anchors.centerIn: parent
                        text: coverUrl.length === 0 ? qsTr("暂无封面") : qsTr("加载中...")
                        font.pixelSize: ThemeProvider.fontSizeSmall
                        color: ThemeProvider.textDisabled
                    }
                }
            }
            
            // 信息列
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignTop
                spacing: ThemeProvider.spacingSmall
                
                // 游戏版本
                RowLayout {
                    spacing: ThemeProvider.spacingSmall
                    Text {
                        text: qsTr("游戏版本")
                        font.pixelSize: ThemeProvider.fontSizeMedium
                        color: ThemeProvider.textSecondary
                        Layout.preferredWidth: 80
                    }
                    Text {
                        text: gameVersion || "-"
                        font.pixelSize: ThemeProvider.fontSizeMedium
                        font.bold: true
                        color: ThemeProvider.textPrimary
                    }
                }
                
                // 选项数量
                RowLayout {
                    spacing: ThemeProvider.spacingSmall
                    Text {
                        text: qsTr("选项数量")
                        font.pixelSize: ThemeProvider.fontSizeMedium
                        color: ThemeProvider.textSecondary
                        Layout.preferredWidth: 100
                    }
                    Text {
                        text: optionsCount + qsTr(" 项")
                        font.pixelSize: ThemeProvider.fontSizeMedium
                        font.bold: true
                        color: ThemeProvider.textPrimary
                    }
                }
                
                // 最后更新
                RowLayout {
                    spacing: ThemeProvider.spacingSmall
                    Text {
                        text: qsTr("最后更新")
                        font.pixelSize: ThemeProvider.fontSizeMedium
                        color: ThemeProvider.textSecondary
                        Layout.preferredWidth: 100
                    }
                    Text {
                        text: lastUpdate
                        font.pixelSize: ThemeProvider.fontSizeMedium
                        font.bold: true
                        color: ThemeProvider.textPrimary
                    }
                }
                
                Item { Layout.fillHeight: true }
            }
        }
        
        // 版本选择区
        GroupBox {
            Layout.fillWidth: true
            title: qsTr("版本选择")
            
            background: Rectangle {
                y: parent.topPadding - parent.padding
                width: parent.width
                height: parent.height - parent.topPadding + parent.padding
                radius: ThemeProvider.radiusMedium
                color: ThemeProvider.cardColor
                border.width: 1
                border.color: ThemeProvider.borderColor
            }
            
            label: Text {
                x: parent.leftPadding
                text: parent.title
                font.pixelSize: ThemeProvider.fontSizeMedium
                font.bold: true
                color: ThemeProvider.textPrimary
            }
            
            StyledComboBox {
                width: parent.width
                model: versions.map(v => v.name || v)
                currentIndex: selectedVersionIndex
                onCurrentIndexChanged: versionChanged(currentIndex)
            }
        }
        
        // 修改器选项显示区
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            TextArea {
                id: optionsText
                readOnly: true
                wrapMode: TextArea.Wrap
                text: optionsHtml
                font.pixelSize: ThemeProvider.fontSizeMedium
                color: ThemeProvider.textPrimary
                placeholderText: qsTr("修改器功能选项列表...")
                
                background: Rectangle {
                    radius: ThemeProvider.radiusMedium
                    color: ThemeProvider.surfaceColor
                    border.width: 1
                    border.color: ThemeProvider.borderColor
                }
            }
        }
        
        // 下载进度条
        ProgressIndicator {
            Layout.fillWidth: true
            visible: downloading
            value: downloadProgress
            statusText: downloading ? qsTr("下载中...") + Math.round(downloadProgress * 100) + "%" : ""
        }
        
        // 按钮区 - 仅图标
        RowLayout {
            Layout.fillWidth: true
            spacing: ThemeProvider.spacingLarge
            
            // 下载按钮
            Rectangle {
                width: 36
                height: 36
                radius: ThemeProvider.radiusSmall
                color: downloadMouseArea.containsMouse ? ThemeProvider.hoverColor : "transparent"
                enabled: !downloading && versions.length > 0
                opacity: enabled ? 1.0 : 0.5
                
                Image {
                    anchors.centerIn: parent
                    source: "qrc:/icons/download.png"
                    width: 20
                    height: 20
                    sourceSize: Qt.size(20, 20)
                }
                
                MouseArea {
                    id: downloadMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: if (parent.enabled) downloadRequested(selectedVersionIndex)
                }
            }
            
            // 打开文件夹按钮
            Rectangle {
                width: 36
                height: 36
                radius: ThemeProvider.radiusSmall
                color: folderMouseArea.containsMouse ? ThemeProvider.hoverColor : "transparent"
                
                Image {
                    anchors.centerIn: parent
                    source: "qrc:/icons/folder.png"
                    width: 20
                    height: 20
                    sourceSize: Qt.size(20, 20)
                }
                
                MouseArea {
                    id: folderMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: openFolderRequested()
                }
            }
            
            // 设置按钮
            Rectangle {
                width: 36
                height: 36
                radius: ThemeProvider.radiusSmall
                color: settingsMouseArea.containsMouse ? ThemeProvider.hoverColor : "transparent"
                
                Image {
                    anchors.centerIn: parent
                    source: "qrc:/icons/settings.png"
                    width: 20
                    height: 20
                    sourceSize: Qt.size(20, 20)
                }
                
                MouseArea {
                    id: settingsMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settingsRequested()
                }
            }
            
            Item { Layout.fillWidth: true }
        }
    }
}
