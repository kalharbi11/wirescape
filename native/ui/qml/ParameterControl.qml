import QtQuick
import QtQuick.Controls

Item {
    id: control
    required property var descriptor
    required property int nodeId
    readonly property real lo: Number(descriptor.minimum)
    readonly property real hi: Number(descriptor.maximum)
    readonly property bool logarithmic: lo > 0 && hi / lo >= 20 && (descriptor.unit === "Hz" || descriptor.unit === "s")
    property real acceptedValue: Number(descriptor.value)
    property string acceptedDisplay: descriptor.display
    objectName: "parameter_" + nodeId + "_" + descriptor.id
    height: 67
    function normalized(v) { if (hi === lo) return 0; return logarithmic ? Math.log(v / lo) / Math.log(hi / lo) : (v - lo) / (hi - lo); }
    function raw(v) { return logarithmic ? lo * Math.pow(hi / lo, v) : lo + (hi - lo) * v; }
    Text { x: 0; y: 4; text: control.descriptor.name.toUpperCase(); color: "#858681"; font.family: (typeof studioFontFamily !== "undefined" ? studioFontFamily : "Arial"); font.pixelSize: 9; font.letterSpacing: 0.8 }
    Text {
        anchors.right: parent.right; y: 2
        objectName: "parameterDisplay"
        text: control.acceptedDisplay
        color: "#393b39"; font.family: (typeof studioFontFamily !== "undefined" ? studioFontFamily : "Arial"); font.pixelSize: 12
    }
    Slider {
        id: slider
        objectName: "parameterSlider"
        enabled: control.hi > control.lo
        anchors.left: parent.left; anchors.right: parent.right; y: 20; height: 34
        from: 0; to: 1
        value: control.normalized(control.acceptedValue)
        onMoved: {
            if (!studio.setParameter(control.nodeId, control.descriptor.id, control.raw(value)))
                value = control.normalized(control.acceptedValue);
        }
        background: Rectangle {
            x: slider.leftPadding; y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth; height: 3; radius: 2; color: "#e4e1da"
            Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: 2; color: "#c5774c" }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 18; height: 24; radius: 5; color: "#393b39"
            border.color: slider.activeFocus ? "#c5774c" : "#393b39"
            Rectangle { anchors.centerIn: parent; width: 2; height: 10; color: "#faf9f6"; radius: 1 }
        }
        ToolTip.visible: hovered
        ToolTip.text: control.descriptor.name + " · " + control.lo + "–" + control.hi + " " + control.descriptor.unit
    }
    Connections {
        target: studio
        function onParameterChanged(node, parameter, value, display) {
            if (node !== control.nodeId || parameter !== control.descriptor.id) return;
            control.acceptedValue = value;
            control.acceptedDisplay = display;
            if (!slider.pressed) slider.value = control.normalized(value);
        }
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#e9e7e1" }
}
