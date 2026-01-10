pragma Singleton
import QtQuick

/**
 * ThemeProvider - 主题管理单例
 * 提供统一的主题属性供所有QML组件使用
 * 主题索引:
 *   0=Light(浅色), 1=Dark(深色), 2=Ocean(海洋), 3=Sunset(日落),
 *   4=Forest(森林), 5=Lavender(薰衣草), 6=Rose(玫瑰), 7=Midnight(午夜), 8=Mocha(摩卡)
 */
QtObject {
    id: themeProvider
    
    // 当前主题索引
    // 从 C++ 传入的 initialTheme 上下文属性初始化（支持持久化）
    property int currentTheme: typeof initialTheme !== "undefined" ? initialTheme : 0
    
    // ========== 主色调 ==========
    readonly property color primaryColor: {
        switch(currentTheme) {
            case 0: return "#2196F3"  // Light - 明亮蓝
            case 1: return "#64B5F6"  // Dark - 柔和蓝
            case 2: return "#00BCD4"  // Ocean - 青色
            case 3: return "#FF7043"  // Sunset - 珊瑚橙
            case 4: return "#4CAF50"  // Forest - 森林绿
            case 5: return "#9C7CF4"  // Lavender - 薰衣草紫
            case 6: return "#E91E63"  // Rose - 玫瑰粉
            case 7: return "#5C6BC0"  // Midnight - 靛蓝
            case 8: return "#8D6E63"  // Mocha - 摩卡棕
            default: return "#2196F3"
        }
    }
    
    readonly property color secondaryColor: {
        switch(currentTheme) {
            case 0: return "#4CAF50"  // Light - 清新绿
            case 1: return "#81C784"  // Dark - 柔和绿
            case 2: return "#26C6DA"  // Ocean - 青绿
            case 3: return "#FFAB91"  // Sunset - 浅珊瑚
            case 4: return "#81C784"  // Forest - 浅绿
            case 5: return "#B39DDB"  // Lavender - 浅紫
            case 6: return "#F48FB1"  // Rose - 浅粉
            case 7: return "#7986CB"  // Midnight - 浅靛蓝
            case 8: return "#A1887F"  // Mocha - 浅棕
            default: return "#4CAF50"
        }
    }
    
    // ========== 背景色 ==========
    readonly property color backgroundColor: {
        switch(currentTheme) {
            case 0: return "#FAFAFA"  // Light - 纯白偏暖
            case 1: return "#1A1D23"  // Dark - 深海军蓝
            case 2: return "#E0F7FA"  // Ocean - 浅青背景
            case 3: return "#FFF8E1"  // Sunset - 暖奶油色
            case 4: return "#E8F5E9"  // Forest - 浅绿背景
            case 5: return "#F3E5F5"  // Lavender - 浅紫背景
            case 6: return "#FCE4EC"  // Rose - 浅粉背景
            case 7: return "#1A1A2E"  // Midnight - 深蓝黑
            case 8: return "#EFEBE9"  // Mocha - 浅棕背景
            default: return "#FAFAFA"
        }
    }
    
    readonly property color surfaceColor: {
        switch(currentTheme) {
            case 0: return "#FFFFFF"  // Light
            case 1: return "#252A34"  // Dark - 深蓝卡片
            case 2: return "#FFFFFF"  // Ocean
            case 3: return "#FFFDE7"  // Sunset - 浅黄
            case 4: return "#FFFFFF"  // Forest
            case 5: return "#FFFFFF"  // Lavender
            case 6: return "#FFFFFF"  // Rose
            case 7: return "#16213E"  // Midnight - 深蓝卡片
            case 8: return "#FFFFFF"  // Mocha
            default: return "#FFFFFF"
        }
    }
    
    readonly property color cardColor: {
        switch(currentTheme) {
            case 0: return "#FFFFFF"
            case 1: return "#2D3340"  // Dark - 卡片蓝
            case 2: return "#FFFFFF"
            case 3: return "#FFFFFF"
            case 4: return "#FFFFFF"
            case 5: return "#FFFFFF"
            case 6: return "#FFFFFF"
            case 7: return "#1F2940"  // Midnight - 卡片深蓝
            case 8: return "#FFFFFF"
            default: return "#FFFFFF"
        }
    }
    
    // ========== 输入框背景 ==========
    readonly property color inputBackground: {
        switch(currentTheme) {
            case 0: return "#FFFFFF"
            case 1: return "#1E2128"  // Dark - 深色输入框
            case 2: return "#FFFFFF"
            case 3: return "#FFFFFF"
            case 4: return "#FFFFFF"
            case 5: return "#FFFFFF"
            case 6: return "#FFFFFF"
            case 7: return "#0F0F1A"  // Midnight - 深色输入框
            case 8: return "#FFFFFF"
            default: return "#FFFFFF"
        }
    }
    
    // ========== 文本色 ==========
    readonly property color textPrimary: {
        switch(currentTheme) {
            case 0: return "#212121"  // Light - 深黑
            case 1: return "#ECEFF1"  // Dark - 浅白
            case 2: return "#006064"  // Ocean - 深青
            case 3: return "#4E342E"  // Sunset - 深棕
            case 4: return "#1B5E20"  // Forest - 深绿
            case 5: return "#4A148C"  // Lavender - 深紫
            case 6: return "#880E4F"  // Rose - 深粉
            case 7: return "#E8EAF6"  // Midnight - 浅白
            case 8: return "#3E2723"  // Mocha - 深棕
            default: return "#212121"
        }
    }
    
    readonly property color textSecondary: {
        switch(currentTheme) {
            case 0: return "#555555"  // Light - 深灰替代
            case 1: return "#B0BEC5"  // Dark - 柔和白
            case 2: return "#00838F"  // Ocean - 中青
            case 3: return "#6D4C41"  // Sunset - 中棕
            case 4: return "#388E3C"  // Forest - 中绿
            case 5: return "#7B1FA2"  // Lavender - 中紫
            case 6: return "#C2185B"  // Rose - 中粉
            case 7: return "#9FA8DA"  // Midnight - 柔和白
            case 8: return "#5D4037"  // Mocha - 中棕
            default: return "#555555"
        }
    }
    
    readonly property color textDisabled: {
        switch(currentTheme) {
            case 0: return "#BDBDBD"  // Light - 浅色
            case 1: return "#546E7A"  // Dark - 蓝灰
            case 2: return "#80CBC4"  // Ocean - 浅青
            case 3: return "#BCAAA4"  // Sunset - 浅棕
            case 4: return "#A5D6A7"  // Forest - 浅绿
            case 5: return "#CE93D8"  // Lavender - 浅紫
            case 6: return "#F8BBD9"  // Rose - 浅粉
            case 7: return "#5C6BC0"  // Midnight - 蓝灰
            case 8: return "#BCAAA4"  // Mocha - 浅棕
            default: return "#BDBDBD"
        }
    }
    
    // ========== 边框色 ==========
    readonly property color borderColor: {
        switch(currentTheme) {
            case 0: return "#E0E0E0"  // Light
            case 1: return "#3D4450"  // Dark - 深蓝边框
            case 2: return "#B2EBF2"  // Ocean - 浅青边框
            case 3: return "#FFCCBC"  // Sunset - 浅橙边框
            case 4: return "#C8E6C9"  // Forest - 浅绿边框
            case 5: return "#E1BEE7"  // Lavender - 浅紫边框
            case 6: return "#F8BBD9"  // Rose - 浅粉边框
            case 7: return "#3949AB"  // Midnight - 靛蓝边框
            case 8: return "#D7CCC8"  // Mocha - 浅棕边框
            default: return "#E0E0E0"
        }
    }
    
    // ========== 悬停/选中色 ==========
    readonly property color hoverColor: {
        switch(currentTheme) {
            case 0: return "#E3F2FD"  // Light - 浅蓝悬停
            case 1: return "#37474F"  // Dark - 深蓝悬停
            case 2: return "#B2EBF2"  // Ocean - 青悬停
            case 3: return "#FFE0B2"  // Sunset - 橙悬停
            case 4: return "#C8E6C9"  // Forest - 浅绿悬停
            case 5: return "#E1BEE7"  // Lavender - 浅紫悬停
            case 6: return "#F8BBD9"  // Rose - 浅粉悬停
            case 7: return "#283593"  // Midnight - 深靛蓝悬停
            case 8: return "#D7CCC8"  // Mocha - 浅棕悬停
            default: return "#E3F2FD"
        }
    }
    
    readonly property color selectedColor: {
        switch(currentTheme) {
            case 0: return "#BBDEFB"  // Light - 蓝选中
            case 1: return "#455A64"  // Dark - 蓝选中
            case 2: return "#80DEEA"  // Ocean - 青选中
            case 3: return "#FFCC80"  // Sunset - 橙选中
            case 4: return "#A5D6A7"  // Forest - 绿选中
            case 5: return "#CE93D8"  // Lavender - 紫选中
            case 6: return "#F48FB1"  // Rose - 粉选中
            case 7: return "#3F51B5"  // Midnight - 靛蓝选中
            case 8: return "#BCAAA4"  // Mocha - 棕选中
            default: return "#BBDEFB"
        }
    }
    
    // ========== 交替行色 ==========
    readonly property color alternateRowColor: {
        switch(currentTheme) {
            case 0: return "#F5F5F5"  // Light
            case 1: return "#2A2F3A"  // Dark
            case 2: return "#E0F7FA"  // Ocean
            case 3: return "#FFF3E0"  // Sunset
            case 4: return "#E8F5E9"  // Forest
            case 5: return "#F3E5F5"  // Lavender
            case 6: return "#FCE4EC"  // Rose
            case 7: return "#1A237E"  // Midnight
            case 8: return "#EFEBE9"  // Mocha
            default: return "#F5F5F5"
        }
    }
    
    // ========== 禁用背景色 ==========
    readonly property color disabledColor: {
        switch(currentTheme) {
            case 0: return "#E0E0E0"
            case 1: return "#37474F"
            case 2: return "#B2DFDB"
            case 3: return "#D7CCC8"
            case 4: return "#C8E6C9"
            case 5: return "#E1BEE7"
            case 6: return "#F8BBD9"
            case 7: return "#303F9F"
            case 8: return "#D7CCC8"
            default: return "#E0E0E0"
        }
    }
    
    // ========== 功能色 ==========
    readonly property color successColor: "#4CAF50"
    readonly property color warningColor: "#FF9800"
    readonly property color errorColor: "#F44336"
    readonly property color dangerColor: "#F44336"  // Same as errorColor, used for delete buttons
    readonly property color infoColor: "#2196F3"
    
    // ========== 尺寸 ==========
    readonly property int radiusSmall: 4
    readonly property int radiusMedium: 6
    readonly property int radiusLarge: 8
    
    readonly property int spacingSmall: 5
    readonly property int spacingMedium: 10
    readonly property int spacingLarge: 15
    
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeMedium: 14
    readonly property int fontSizeLarge: 16
    readonly property int fontSizeTitle: 18
    
    // ========== 主题名称 ==========
    readonly property var themeNames: [
        qsTr("浅色"),
        qsTr("深色"),
        qsTr("海洋"),
        qsTr("日落"),
        qsTr("森林"),
        qsTr("薰衣草"),
        qsTr("玫瑰"),
        qsTr("午夜"),
        qsTr("摩卡")
    ]
    
    // Resource helper: build absolute qrc path inside module
    function assetUrl(path) {
        return "qrc:/resources/" + path
    }

    function getThemeName(index) {
        return themeNames[index] || themeNames[0]
    }
}
