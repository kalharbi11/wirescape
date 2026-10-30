import QtQuick
import QtQuick.Controls

Item {
    id: control
    required property var descriptor
    required property int nodeId
    // Presentation metadata is UI-only; descriptor bounds, IDs and accepted values are authoritative.
    property string presentation: "slider" // slider | fader | knob | stepper
    property string hierarchy: "regular"  // primary | related | regular
    property bool discrete: false
    readonly property real lo: Number(descriptor.minimum)
    readonly property real hi: Number(descriptor.maximum)
    readonly property bool logarithmic: lo > 0 && hi / lo >= 20 &&
        (descriptor.unit === "Hz" || descriptor.unit === "s")
    property real acceptedValue: Number(descriptor.value)
    property string acceptedDisplay: descriptor.display
    objectName: "parameter_" + nodeId + "_" + descriptor.id
    implicitHeight: presentation === "knob" ? (hierarchy === "primary" ? 154 : 125)
                    : presentation === "fader" ? 168 : 67
    function normalized(v) {
        if (!(hi > lo)) return 0;
        var n = logarithmic ? Math.log(Math.max(lo, v) / lo) / Math.log(hi / lo)
                            : (v - lo) / (hi - lo);
        return Math.max(0, Math.min(1, n));
    }
    function raw(n) {
        n = Math.max(0, Math.min(1, n));
        var v = logarithmic ? lo * Math.pow(hi / lo, n) : lo + (hi - lo) * n;
        return discrete ? Math.max(lo, Math.min(hi, Math.round(v))) : v;
    }
    function submit(n) {
        if (!(hi > lo)) return false;
        return studio.setParameter(nodeId, descriptor.id, raw(n));
    }
    function nudge(direction, fine) {
        var size = discrete ? 1 / (hi - lo) : (fine ? 0.001 : 0.01);
        submit(normalized(acceptedValue) + direction * size);
    }
    onDescriptorChanged: {
        acceptedValue = Number(descriptor.value);
        acceptedDisplay = descriptor.display;
    }

    Text {
        x: 0; y: 4; width: parent.width * 0.53; elide: Text.ElideRight
        text: control.descriptor.name.toUpperCase()
        color: "#858681"; font.family: (typeof studioFontFamily !== "undefined" ? studioFontFamily : "Arial")
        font.pixelSize: 9; font.letterSpacing: 0.8
    }
    Text {
        anchors.right: parent.right; y: 2; width: parent.width * 0.47
        horizontalAlignment: Text.AlignRight; elide: Text.ElideRight
        objectName: "parameterDisplay"
        text: control.acceptedDisplay
        color: "#393b39"; font.family: (typeof studioFontFamily !== "undefined" ? studioFontFamily : "Arial")
        font.pixelSize: 12
    }
    Loader {
        id: input
        y: 21; width: parent.width
        height: control.implicitHeight - 29
        sourceComponent: control.presentation === "knob" ? knob :
                         control.presentation === "stepper" ? stepper : slider
    }
    Component {
        id: knob
        RotaryControl {
            width: height; height: control.hierarchy === "primary" ? 114 : 85
            x: (input.width - width) / 2
            diameter: height
            enabled: control.hi > control.lo
            value: control.normalized(control.acceptedValue)
            label: control.descriptor.name
            valueText: control.acceptedDisplay
            onMoved: function(position, fine) { control.submit(position); }
            ToolTip.visible: activeFocus || hover.hovered
            ToolTip.text: label + " · " + control.lo + "–" + control.hi + " " + control.descriptor.unit
            HoverHandler { id: hover }
        }
    }
    Component {
        id: stepper
        SpinBox {
            objectName: "parameterStepper"
            from: Math.ceil(control.lo); to: Math.floor(control.hi)
            enabled: control.hi > control.lo
            value: Math.round(control.acceptedValue)
            editable: false
            width: input.width
            onValueModified: {
                if (!control.submit(control.normalized(value)))
                    value = Math.round(control.acceptedValue);
            }
            Accessible.name: control.descriptor.name
            Accessible.description: control.acceptedDisplay
        }
    }
    Component {
        id: slider
        Slider {
            id: range
            objectName: "parameterSlider"
            readonly property bool vertical: control.presentation === "fader"
            width: vertical ? Math.min(72, input.width) : input.width
            height: vertical ? input.height : 34
            x: vertical ? (input.width - width) / 2 : 0
            orientation: vertical ? Qt.Vertical : Qt.Horizontal
            enabled: control.hi > control.lo
            from: 0; to: 1
            stepSize: control.discrete ? 1 / (control.hi - control.lo) : 0
            value: control.normalized(control.acceptedValue)
            onMoved: {
                if (!control.submit(value)) value = control.normalized(control.acceptedValue);
            }
            onPressedChanged: if (!pressed) value = control.normalized(control.acceptedValue)
            Keys.onPressed: function(event) {
                var direction = event.key === Qt.Key_Up || event.key === Qt.Key_Right ? 1 :
                                event.key === Qt.Key_Down || event.key === Qt.Key_Left ? -1 : 0;
                if (direction !== 0) {
                    control.nudge(direction, (event.modifiers & Qt.ShiftModifier) !== 0);
                    event.accepted = true;
                }
            }
            Accessible.name: control.descriptor.name
            Accessible.description: control.acceptedDisplay
            background: Rectangle {
                x: range.vertical ? range.width / 2 - width / 2 : range.leftPadding
                y: range.vertical ? range.topPadding : range.topPadding + range.availableHeight / 2 - height / 2
                width: range.vertical ? 3 : range.availableWidth
                height: range.vertical ? range.availableHeight : 3
                radius: 2; color: "#e4e1da"
                Rectangle {
                    x: 0
                    y: range.vertical ? parent.height * (1 - range.visualPosition) : 0
                    width: range.vertical ? parent.width : range.visualPosition * parent.width
                    height: range.vertical ? range.visualPosition * parent.height : parent.height
                    radius: 2; color: "#c5774c"
                }
            }
            handle: Rectangle {
                x: range.vertical ? (range.width - width) / 2 :
                   range.leftPadding + range.visualPosition * (range.availableWidth - width)
                y: range.vertical ? range.topPadding + (1 - range.visualPosition) * (range.availableHeight - height) :
                   range.topPadding + range.availableHeight / 2 - height / 2
                width: range.vertical ? 31 : 18; height: range.vertical ? 16 : 24
                radius: 5; color: "#393b39"
                border.color: range.activeFocus ? "#c5774c" : "#393b39"
                Rectangle {
                    anchors.centerIn: parent
                    width: range.vertical ? 12 : 2; height: range.vertical ? 2 : 10
                    color: "#faf9f6"; radius: 1
                }
            }
            ToolTip.visible: hovered
            ToolTip.text: control.descriptor.name + " · " + control.lo + "–" + control.hi + " " + control.descriptor.unit
        }
    }
    Connections {
        target: studio
        function onParameterChanged(node, parameter, value, display) {
            if (node !== control.nodeId || parameter !== control.descriptor.id) return;
            control.acceptedValue = value;
            control.acceptedDisplay = display;
            // Loader children are never recreated just to update a value.
            if (input.item && input.item.objectName === "parameterSlider" && !input.item.pressed)
                input.item.value = control.normalized(value);
        }
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#e9e7e1" }
}
