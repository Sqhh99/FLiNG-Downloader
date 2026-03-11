import QtQuick
import QtQuick.Controls.Basic
import FLiNGDownloader

/**
 * StyledSwitch - Theme-aware switch control
 */
Switch {
    id: control

    implicitWidth: 46
    implicitHeight: 26

    padding: 0
    spacing: 0

    indicator: Rectangle {
        implicitWidth: 46
        implicitHeight: 26
        radius: height / 2
        border.width: 1
        border.color: control.checked ? ThemeProvider.primaryColor : ThemeProvider.borderColor
        color: {
            if (!control.enabled) return ThemeProvider.disabledColor
            if (control.checked) return Qt.tint(ThemeProvider.primaryColor, Qt.rgba(1, 1, 1, 0.2))
            if (control.pressed) return ThemeProvider.hoverColor
            return ThemeProvider.backgroundColor
        }

        Behavior on color {
            ColorAnimation { duration: 120 }
        }

        Behavior on border.color {
            ColorAnimation { duration: 120 }
        }

        Rectangle {
            id: thumb
            width: 20
            height: 20
            radius: 10
            y: 3
            x: control.checked ? parent.width - width - 3 : 3
            color: control.enabled ? ThemeProvider.surfaceColor : ThemeProvider.textDisabled
            border.width: 1
            border.color: control.checked ? ThemeProvider.primaryColor : ThemeProvider.borderColor

            Behavior on x {
                NumberAnimation { duration: 120; easing.type: Easing.InOutQuad }
            }

            Behavior on border.color {
                ColorAnimation { duration: 120 }
            }
        }
    }

    contentItem: Item {}
    background: Item {}
}
