import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool accent: false
    property bool quiet: false
    implicitHeight: 34
    implicitWidth: Math.max(54, label.implicitWidth + 24)
    font.family: (typeof studioFontFamily !== "undefined" ? studioFontFamily : "Arial")
    font.pixelSize: 11
    hoverEnabled: true
    contentItem: Text {
        id: label
        text: control.text
        font: control.font
        color: control.accent ? "#faf9f6" : (control.enabled ? "#393b39" : "#a7a8a1")
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 7
        color: control.accent ? (control.down ? "#a75f3b" : "#c5774c") : (control.down ? "#e9e7e1" : control.hovered ? "#faf9f6" : control.quiet ? "transparent" : "#f5f4f0")
        border.color: control.activeFocus ? "#c5774c" : control.quiet && !control.hovered ? "transparent" : "#d9d8d2"
    }
}
