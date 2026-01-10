import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FLiNGDownloader

/**
 * SearchPage - 搜索标签页
 * 包含搜索栏、排序选项和修改器列表（带详情按钮）
 * 下载功能已移至详情面板
 */
Item {
    id: searchPage
    
    // 后端接口和模型
    property var backend: null  // 后端对象 (getSuggestions 等方法)
    property var modifierModel: null
    
    // 信号
    signal modifierSelected(int index)
    signal searchRequested(string keyword)
    signal sortChanged(int sortIndex)
    signal refreshRequested()
    signal detailsRequested(int index)
    
    // 标记：是否正在程序化设置文本（避免触发建议列表）
    property bool isProgrammaticTextChange: false
    
    // 更新建议列表的函数 - 从游戏数据库获取建议（支持中英文）
    function updateSuggestions(keyword) {
        suggestionsModel.clear()
        if (!backend || keyword.length < 1) {
            return
        }
        
        // 使用 backend 的 getSuggestions 方法从 game_mappings.json 获取建议
        var suggestions = backend.getSuggestions(keyword, 8)
        console.log("updateSuggestions:", keyword, "-> found", suggestions.length, "suggestions")
        
        for (var i = 0; i < suggestions.length; i++) {
            suggestionsModel.append({modelData: suggestions[i]})
        }
    }
    
    // 建议列表模型 - 放在根Item下以确保可访问性
    ListModel {
        id: suggestionsModel
    }
    
    // 设置选中的行
    function selectRow(index) {
        modifierTable.currentIndex = index
    }
    
    // 监控模型变化
    Connections {
        target: modifierModel
        function onCountChanged() {
            console.log("SearchPage: 模型数据变化，当前行数:", modifierModel ? modifierModel.rowCount() : 0)
        }
    }
    
    onModifierModelChanged: {
        console.log("SearchPage: modifierModel 属性变化:", modifierModel)
        if (modifierModel) {
            console.log("SearchPage: 模型行数:", modifierModel.rowCount())
        }
    }
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: ThemeProvider.spacingMedium
        spacing: ThemeProvider.spacingMedium
        
        // 搜索栏容器
        Item {
            Layout.fillWidth: true
            height: searchRow.height
            
            RowLayout {
                id: searchRow
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: ThemeProvider.spacingMedium
                
                StyledTextField {
                    id: searchInput
                    Layout.fillWidth: true
                    placeholderText: qsTr("搜索游戏...")
                    showClearButton: true
                    
                    // 键盘导航支持
                    Keys.onReturnPressed: {
                        if (suggestionsPopup.opened && suggestionsList.currentIndex >= 0) {
                            // 如果建议列表可见且有选中项，选择该项
                            suggestionsList.currentItem.selectSuggestion()
                        } else {
                            suggestionsPopup.close()
                            searchRequested(text)
                        }
                    }
                    Keys.onEnterPressed: {
                        if (suggestionsPopup.opened && suggestionsList.currentIndex >= 0) {
                            suggestionsList.currentItem.selectSuggestion()
                        } else {
                            suggestionsPopup.close()
                            searchRequested(text)
                        }
                    }
                    Keys.onEscapePressed: {
                        suggestionsPopup.close()
                        suggestionsList.currentIndex = -1
                    }
                    Keys.onDownPressed: {
                        if (suggestionsPopup.opened) {
                            suggestionsList.currentIndex = Math.min(suggestionsList.currentIndex + 1, suggestionsModel.count - 1)
                        }
                    }
                    Keys.onUpPressed: {
                        if (suggestionsPopup.opened) {
                            suggestionsList.currentIndex = Math.max(suggestionsList.currentIndex - 1, 0)
                        }
                    }
                    
                    onTextChanged: {
                        // 如果是程序化设置文本，不触发建议列表
                        if (searchPage.isProgrammaticTextChange) {
                            console.log("searchInput.onTextChanged: (programmatic, skipped)")
                            return
                        }
                        
                        console.log("searchInput.onTextChanged:", text, "activeFocus:", activeFocus)
                        if (text.length >= 1 && activeFocus) {
                            // 过滤建议列表
                            searchPage.updateSuggestions(text)
                            console.log("suggestionsModel.count:", suggestionsModel.count)
                            if (suggestionsModel.count > 0) {
                                suggestionsPopup.open()
                            } else {
                                suggestionsPopup.close()
                            }
                            console.log("suggestionsPopup.opened:", suggestionsPopup.opened, "height:", suggestionsPopup.height)
                        } else {
                            suggestionsPopup.close()
                        }
                    }
                    
                    onActiveFocusChanged: {
                        if (!activeFocus) {
                            // 延迟隐藏，允许点击建议
                            hideTimer.start()
                        }
                    }
                }
                
                StyledButton {
                    text: qsTr("搜索")
                    buttonType: "secondary"
                    onClicked: {
                        suggestionsPopup.close()
                        if (searchInput.text.trim() === "") {
                            refreshRequested()
                        } else {
                            searchRequested(searchInput.text)
                        }
                    }
                }
                
                StyledComboBox {
                    id: sortComboBox
                    implicitWidth: 120
                    model: [qsTr("最近更新"), qsTr("按名称"), qsTr("选项数量")]
                    onActivated: function(index) {
                        console.log("排序方式改变:", index)
                        sortChanged(index)
                    }
                }
                
                StyledButton {
                    text: qsTr("显示全部")
                    buttonType: "primary"
                    onClicked: {
                        searchInput.clear()
                        suggestionsPopup.close()
                        refreshRequested()
                    }
                }
            }
            
            // 延迟隐藏计时器
            Timer {
                id: hideTimer
                interval: 150
                onTriggered: {
                    suggestionsPopup.close()
                }
            }
        }
        
        // 搜索建议弹出框 - 使用 Popup 确保正确覆盖
        Popup {
            id: suggestionsPopup
            
            // 定位到搜索框下方
            x: searchInput.mapToItem(searchPage, 0, 0).x
            y: searchInput.mapToItem(searchPage, 0, searchInput.height + 4).y
            width: searchInput.width
            height: Math.min(suggestionsModel.count * 36 + 8, 208)
            
            padding: 4
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
            
            background: Rectangle {
                color: ThemeProvider.surfaceColor
                border.color: ThemeProvider.borderColor
                border.width: 1
                radius: ThemeProvider.radiusSmall
                
                // 阴影效果
                layer.enabled: true
                layer.effect: null
            }
            
            contentItem: ListView {
                id: suggestionsList
                implicitHeight: contentHeight
                model: suggestionsModel
                clip: true
                currentIndex: -1  // 初始无选中
                
                // 重置当前选中项当建议列表更新时
                onCountChanged: currentIndex = -1
                
                delegate: Rectangle {
                    id: suggestionDelegate
                    width: suggestionsList.width
                    height: 32
                    // 键盘选中或鼠标悬停时高亮
                    color: {
                        if (index === suggestionsList.currentIndex)
                            return ThemeProvider.selectedColor
                        if (suggestionMouseArea.containsMouse)
                            return ThemeProvider.hoverColor
                        return "transparent"
                    }
                    radius: ThemeProvider.radiusSmall
                    
                    // 提供给键盘选择调用的函数
                    function selectSuggestion() {
                        var searchKeyword = modelData
                        var parenStart = modelData.indexOf("(")
                        var parenEnd = modelData.indexOf(")")
                        if (parenStart !== -1 && parenEnd !== -1) {
                            var inParens = modelData.substring(parenStart + 1, parenEnd)
                            var mainPart = modelData.substring(0, parenStart).trim()
                            if (/[\u4e00-\u9fa5]/.test(mainPart)) {
                                searchKeyword = inParens
                            } else {
                                searchKeyword = mainPart
                            }
                        }
                        
                        searchPage.isProgrammaticTextChange = true
                        searchInput.text = searchKeyword
                        searchPage.isProgrammaticTextChange = false
                        
                        suggestionsPopup.close()
                        suggestionsList.currentIndex = -1
                        searchRequested(searchKeyword)
                    }
                    
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData
                        font.pixelSize: ThemeProvider.fontSizeMedium
                        color: ThemeProvider.textPrimary
                        elide: Text.ElideRight
                        width: parent.width - 20
                    }
                    
                    MouseArea {
                        id: suggestionMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: suggestionDelegate.selectSuggestion()
                    }
                }
            }
        }
        
        // 修改器表格
        StyledTable {
            id: modifierTable
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            model: searchPage.modifierModel
            
            headers: [qsTr("游戏名称"), qsTr("更新日期"), qsTr("支持版本"), qsTr("选项数量"), qsTr("操作")]
            columnWidths: [240, 100, 130, 80, 80]
            
            delegate: Rectangle {
                id: delegateRoot
                width: modifierTable.width
                height: modifierTable.rowHeight
                
                // 存储当前行索引
                property int rowIndex: index
                
                color: {
                    if (modifierTable.currentIndex === rowIndex) 
                        return ThemeProvider.selectedColor
                    if (rowMouseArea.containsMouse)
                        return ThemeProvider.hoverColor
                    if (rowIndex % 2 === 1)
                        return ThemeProvider.alternateRowColor
                    return "transparent"
                }
                
                // 行选择区域（不包括操作按钮列）
                MouseArea {
                    id: rowMouseArea
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.width - 80  // 减去操作列宽度
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    
                    onClicked: {
                        modifierTable.currentIndex = delegateRoot.rowIndex
                        searchPage.modifierSelected(delegateRoot.rowIndex)
                    }
                    
                    onDoubleClicked: {
                        // 双击打开详情
                        searchPage.detailsRequested(delegateRoot.rowIndex)
                    }
                }
                
                // 数据行
                Row {
                    anchors.fill: parent
                    
                    // 游戏名称
                    Item {
                        width: modifierTable.columnWidths[0]
                        height: parent.height
                        
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            text: model.name || ""
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textPrimary
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }
                    
                    // 更新日期
                    Item {
                        width: modifierTable.columnWidths[1]
                        height: parent.height
                        
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            text: model.lastUpdate || ""
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textSecondary
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }
                    
                    // 支持版本
                    Item {
                        width: modifierTable.columnWidths[2]
                        height: parent.height
                        
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            text: model.gameVersion || ""
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textSecondary
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }
                    
                    // 选项数量
                    Item {
                        width: modifierTable.columnWidths[3]
                        height: parent.height
                        
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            text: model.optionsCount || "0"
                            font.pixelSize: ThemeProvider.fontSizeMedium
                            color: ThemeProvider.textSecondary
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    
                    // 操作按钮区 - 只有详情图标按钮
                    Item {
                        width: modifierTable.columnWidths[4]
                        height: parent.height
                        
                        // 详情按钮 - 仅图标
                        Rectangle {
                            anchors.centerIn: parent
                            width: 28
                            height: 28
                            radius: ThemeProvider.radiusSmall
                            color: detailsMouseArea.containsMouse ? ThemeProvider.hoverColor : "transparent"
                            
                            Image {
                                anchors.centerIn: parent
                                source: ThemeProvider.assetUrl("icons/details.png")
                                width: 18
                                height: 18
                                sourceSize: Qt.size(18, 18)
                            }
                            
                            MouseArea {
                                id: detailsMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                
                                onClicked: {
                                    console.log("详情按钮点击, rowIndex:", delegateRoot.rowIndex)
                                    searchPage.detailsRequested(delegateRoot.rowIndex)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
