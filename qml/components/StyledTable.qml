import QtQuick
import QtQuick.Controls.Basic
import FLiNGDownloader

/**
 * StyledTable - 统一样式表格组件
 * 基于 ListView 实现简单表格功能
 */
Item {
    id: control
    
    property var headers: []
    property var columnWidths: []
    property alias model: listView.model
    property alias delegate: listView.delegate
    property alias currentIndex: listView.currentIndex
    property int headerHeight: 36
    property int rowHeight: 40
    
    signal rowClicked(int index)
    signal rowDoubleClicked(int index)
    
    implicitWidth: 400
    implicitHeight: 300
    
    Rectangle {
        id: container
        anchors.fill: parent
        radius: ThemeProvider.radiusMedium
        color: ThemeProvider.surfaceColor
        border.width: 1
        border.color: ThemeProvider.borderColor
        clip: true
        
        // 表头
        Rectangle {
            id: headerRow
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: headerHeight
            color: ThemeProvider.backgroundColor
            
            Row {
                anchors.fill: parent
                
                Repeater {
                    model: headers
                    
                    Rectangle {
                        width: columnWidths[index] || (control.width / headers.length)
                        height: headerHeight
                        color: "transparent"
                        
                        Text {
                            anchors.fill: parent
                            text: modelData
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            font.bold: true
                            color: ThemeProvider.textSecondary
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                        
                        // 列分隔线
                        Rectangle {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 1
                            color: ThemeProvider.borderColor
                            visible: index < headers.length - 1
                        }
                    }
                }
            }
            
            // 底部分隔线
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: ThemeProvider.borderColor
            }
        }
        
        // 列表内容
        ListView {
            id: listView
            anchors.top: headerRow.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true
            
            ScrollBar.vertical: ScrollBar {
                active: true
            }
            
            // 空状态占位
            Text {
                visible: listView.count === 0
                anchors.centerIn: parent
                text: qsTr("暂无数据")
                font.pixelSize: ThemeProvider.fontSizeMedium
                color: ThemeProvider.textDisabled
            }
        }
    }
}
