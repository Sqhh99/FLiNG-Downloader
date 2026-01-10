import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FLiNGDownloader

/**
 * DownloadedPage - 已下载修改器标签页
 * 列表每行右侧有打开文件夹和删除按钮
 */
Item {
    id: downloadedPage
    
    property alias downloadedModel: downloadedTable.model
    
    signal openFolderRequested(int index)
    signal deleteModifier(int index)
    signal checkUpdates()
    signal modifierDoubleClicked(int index)
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: ThemeProvider.spacingMedium
        spacing: ThemeProvider.spacingMedium
        
        // 已下载表格
        StyledTable {
            id: downloadedTable
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            // 简化为4列：名称、版本、下载日期、操作
            headers: [qsTr("修改器名称"), qsTr("版本"), qsTr("下载日期"), qsTr("操作")]
            columnWidths: [300, 200, 150, 80]
            
            delegate: Item {
                id: delegateRoot
                width: downloadedTable.width
                height: downloadedTable.rowHeight
                
                property int rowIndex: index
                
                // 背景
                Rectangle {
                    anchors.fill: parent
                    color: {
                        if (downloadedTable.currentIndex === delegateRoot.rowIndex)
                            return ThemeProvider.selectedColor
                        if (rowMouseArea.containsMouse)
                            return ThemeProvider.hoverColor
                        if (delegateRoot.rowIndex % 2 === 1)
                            return ThemeProvider.alternateRowColor
                        return "transparent"
                    }
                }
                
                // 数据行
                Row {
                    anchors.fill: parent
                    
                    // 修改器名称
                    Item {
                        width: downloadedTable.columnWidths[0]
                        height: parent.height
                        
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            text: model.name || ""
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textPrimary
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                    
                    // 版本
                    Item {
                        width: downloadedTable.columnWidths[1]
                        height: parent.height
                        
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            text: model.version || ""
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textSecondary
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                    
                    // 下载日期
                    Item {
                        width: downloadedTable.columnWidths[2]
                        height: parent.height
                        
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            text: model.downloadDate || ""
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textSecondary
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    
                    // 操作按钮占位
                    Item {
                        width: downloadedTable.columnWidths[3]
                        height: parent.height
                    }
                }
                
                // 行选择 MouseArea - 只覆盖前三列
                MouseArea {
                    id: rowMouseArea
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: downloadedTable.columnWidths[0] + downloadedTable.columnWidths[1] + downloadedTable.columnWidths[2]
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    
                    onClicked: downloadedTable.currentIndex = delegateRoot.rowIndex
                    onDoubleClicked: modifierDoubleClicked(delegateRoot.rowIndex)
                }
                
                // 操作按钮 - 放在最上层，使用 anchors.right 定位
                Row {
                    id: actionButtons
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 10
                    width: downloadedTable.columnWidths[3]
                    spacing: 8
                    
                    // 居中
                    Item {
                        width: (parent.width - folderBtn.width - deleteBtn.width - 8) / 2
                        height: parent.height
                    }
                    
                    // 打开文件夹按钮
                    Rectangle {
                        id: folderBtn
                        width: 28
                        height: 28
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 4
                        color: folderMouseArea.containsMouse ? ThemeProvider.hoverColor : "transparent"
                        
                        Image {
                            anchors.centerIn: parent
                            source: ThemeProvider.assetUrl("icons/folder.png")
                            width: 18
                            height: 18
                            sourceSize: Qt.size(18, 18)
                        }
                        
                        MouseArea {
                            id: folderMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            
                            onClicked: {
                                console.log("DownloadedPage: 打开文件夹点击, index:", delegateRoot.rowIndex)
                                downloadedPage.openFolderRequested(delegateRoot.rowIndex)
                            }
                        }
                    }
                    
                    // 删除按钮
                    Rectangle {
                        id: deleteBtn
                        width: 28
                        height: 28
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 4
                        color: deleteMouseArea.containsMouse ? Qt.rgba(ThemeProvider.dangerColor.r, ThemeProvider.dangerColor.g, ThemeProvider.dangerColor.b, 0.3) : "transparent"
                        
                        Image {
                            anchors.centerIn: parent
                            source: ThemeProvider.assetUrl("icons/delete.png")
                            width: 18
                            height: 18
                            sourceSize: Qt.size(18, 18)
                        }
                        
                        MouseArea {
                            id: deleteMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            
                            onClicked: {
                                console.log("DownloadedPage: 删除点击, index:", delegateRoot.rowIndex)
                                downloadedPage.deleteModifier(delegateRoot.rowIndex)
                            }
                        }
                    }
                }
            }
        }
    }
}
