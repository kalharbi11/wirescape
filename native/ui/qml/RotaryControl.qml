// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// Normalized presentation only: all writes and accepted values live in ParameterControl.
Item {
    id: dial
    property real value: 0
    property string label: ""
    property string valueText: ""
    property int diameter: 86
    signal moved(real position, bool fine)
    objectName: "parameterKnob"
    implicitWidth: diameter
    implicitHeight: diameter
    activeFocusOnTab: true
    Accessible.role: Accessible.Slider
    Accessible.name: label
    Accessible.description: valueText + "; drag up or down, or use arrow keys; Shift for fine adjustment"
    function step(direction, fine) {
        moved(Math.max(0, Math.min(1, value + direction * (fine ? 0.001 : 0.01))), fine);
    }
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Right ||
            event.key === Qt.Key_Down || event.key === Qt.Key_Left) {
            step(event.key === Qt.Key_Up || event.key === Qt.Key_Right ? 1 : -1,
                 (event.modifiers & Qt.ShiftModifier) !== 0);
            event.accepted = true;
        }
    }
    Canvas {
        id: ring
        anchors.fill: parent
        onPaint: {
            var c = getContext("2d");
            c.clearRect(0, 0, width, height);
            var x = width / 2, y = height / 2, r = Math.min(width, height) / 2 - 8;
            var start = Math.PI * 0.75, sweep = Math.PI * 1.5;
            c.lineCap = "round";
            c.lineWidth = 6;
            c.strokeStyle = "#ddd9d1";
            c.beginPath(); c.arc(x, y, r, start, start + sweep); c.stroke();
            c.strokeStyle = "#c5774c";
            c.beginPath(); c.arc(x, y, r, start, start + sweep * Math.max(0, Math.min(1, dial.value))); c.stroke();
            c.fillStyle = dial.activeFocus ? "#4b4c47" : (gesture.containsMouse ? "#464844" : "#393b39");
            c.beginPath(); c.arc(x, y, r - 9, 0, Math.PI * 2); c.fill();
            var angle = start + sweep * Math.max(0, Math.min(1, dial.value));
            c.strokeStyle = "#faf9f6"; c.lineWidth = 3;
            c.beginPath();
            c.moveTo(x + (r - 21) * Math.cos(angle), y + (r - 21) * Math.sin(angle));
            c.lineTo(x + (r - 12) * Math.cos(angle), y + (r - 12) * Math.sin(angle));
            c.stroke();
            if (dial.activeFocus) {
                c.strokeStyle = "#c5774c"; c.lineWidth = 1;
                c.beginPath(); c.arc(x, y, r + 6, 0, Math.PI * 2); c.stroke();
            }
        }
    }
    onValueChanged: ring.requestPaint()
    onActiveFocusChanged: ring.requestPaint()
    MouseArea {
        id: gesture
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        property real origin: 0
        property real initial: 0
        onEntered: ring.requestPaint()
        onExited: ring.requestPaint()
        onPressed: function(mouse) {
            dial.forceActiveFocus();
            origin = mouse.y;
            initial = dial.value;
        }
        onPositionChanged: function(mouse) {
            if (pressed) dial.moved(Math.max(0, Math.min(1, initial + (origin - mouse.y) /\n                                  ((mouse.modifiers & Qt.ShiftModifier) !== 0 ? 1400 : 140))),
                                  (mouse.modifiers & Qt.ShiftModifier) !== 0);
        }
    }
}
