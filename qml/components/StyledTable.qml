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
    property var columnWeights: []
    property int stretchColumn: -1
    property int headerTextHorizontalAlignment: Text.AlignHCenter
    property int headerTextLeftPadding: 0
    property alias model: listView.model
    property alias delegate: listView.delegate
    property alias currentIndex: listView.currentIndex
    property int headerHeight: 36
    property int rowHeight: 40
    
    signal rowClicked(int index)
    signal rowDoubleClicked(int index)
    
    implicitWidth: 400
    implicitHeight: 300

    function baseColumnWidth(index) {
        var configuredWidth = columnWidths[index]
        if (configuredWidth !== undefined && configuredWidth !== null && configuredWidth > 0) {
            return configuredWidth
        }
        return headers.length > 0 ? Math.floor(control.width / headers.length) : 0
    }

    function columnWidthFor(index) {
        if (columnWeights && columnWeights.length === headers.length) {
            var totalWeight = 0
            for (var i = 0; i < columnWeights.length; ++i) {
                totalWeight += Math.max(0, columnWeights[i] || 0)
            }

            if (totalWeight > 0) {
                if (index === headers.length - 1) {
                    var consumedWidth = 0
                    for (var j = 0; j < index; ++j) {
                        consumedWidth += Math.floor(control.width * Math.max(0, columnWeights[j] || 0) / totalWeight)
                    }
                    return Math.max(0, control.width - consumedWidth)
                }

                return Math.floor(control.width * Math.max(0, columnWeights[index] || 0) / totalWeight)
            }
        }

        var baseWidth = baseColumnWidth(index)
        if (stretchColumn < 0 || stretchColumn >= headers.length) {
            return baseWidth
        }

        var totalBaseWidth = 0
        for (var i = 0; i < headers.length; ++i) {
            totalBaseWidth += baseColumnWidth(i)
        }

        var extraWidth = Math.max(0, control.width - totalBaseWidth)
        if (index === stretchColumn) {
            return baseWidth + extraWidth
        }
        return baseWidth
    }
    
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
                        width: control.columnWidthFor(index)
                        height: headerHeight
                        color: "transparent"
                        
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: control.headerTextHorizontalAlignment === Text.AlignLeft ? control.headerTextLeftPadding : 0
                            text: modelData
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            font.bold: true
                            color: ThemeProvider.textSecondary
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: control.headerTextHorizontalAlignment
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
