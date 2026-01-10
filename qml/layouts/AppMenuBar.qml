import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FLiNGDownloader

/**
 * AppMenuBar - 应用菜单栏
 */
MenuBar {
    id: menuBar
    
    property int currentTheme: 0
    property int currentLanguage: 0
    
    signal themeSelected(int themeIndex)
    signal languageSelected(int languageIndex)
    signal settingsRequested()
    signal exitRequested()
    
    background: Rectangle {
        color: ThemeProvider.backgroundColor
        
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: ThemeProvider.borderColor
        }
    }
    
    delegate: MenuBarItem {
        id: menuBarItem
        
        contentItem: Text {
            text: menuBarItem.text
            font.pixelSize: ThemeProvider.fontSizeMedium
            color: menuBarItem.highlighted ? ThemeProvider.primaryColor : ThemeProvider.textPrimary
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
        }
        
        background: Rectangle {
            color: menuBarItem.highlighted ? ThemeProvider.hoverColor : "transparent"
        }
    }
    
    Menu {
        title: qsTr("文件")
        
        Action {
            text: qsTr("设置")
            onTriggered: settingsRequested()
        }
        
        MenuSeparator {}
        
        Action {
            text: qsTr("退出")
            onTriggered: exitRequested()
        }
    }
    
    Menu {
        title: qsTr("主题")
        
        ActionGroup {
            id: themeGroup
            exclusive: true
        }
        
        Action {
            text: qsTr("浅色主题")
            checkable: true
            checked: currentTheme === 0
            ActionGroup.group: themeGroup
            onTriggered: themeSelected(0)
        }
        
        Action {
            text: qsTr("Windows 11主题")
            checkable: true
            checked: currentTheme === 1
            ActionGroup.group: themeGroup
            onTriggered: themeSelected(1)
        }
        
        Action {
            text: qsTr("经典主题")
            checkable: true
            checked: currentTheme === 2
            ActionGroup.group: themeGroup
            onTriggered: themeSelected(2)
        }
        
        Action {
            text: qsTr("多彩主题")
            checkable: true
            checked: currentTheme === 3
            ActionGroup.group: themeGroup
            onTriggered: themeSelected(3)
        }
    }
    
    Menu {
        title: qsTr("语言")
        
        ActionGroup {
            id: languageGroup
            exclusive: true
        }
        
        Action {
            text: qsTr("中文")
            checkable: true
            checked: currentLanguage === 0
            ActionGroup.group: languageGroup
            onTriggered: languageSelected(0)
        }
        
        Action {
            text: "English"
            checkable: true
            checked: currentLanguage === 1
            ActionGroup.group: languageGroup
            onTriggered: languageSelected(1)
        }
        
        Action {
            text: "日本語"
            checkable: true
            checked: currentLanguage === 2
            ActionGroup.group: languageGroup
            onTriggered: languageSelected(2)
        }
    }
}
