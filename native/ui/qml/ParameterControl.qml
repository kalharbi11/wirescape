import QtQuick
import QtQuick.Controls

Item {
    id: control
    required property var descriptor
    required property int nodeId
    property string moduleId: ""
    readonly property bool isClockRun: moduleId === "essentials.clock" && Number(descriptor.id) === 3
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
    implicitHeight: isClockRun ? 67 : presentation === "knob" ? (hierarchy === "primary" ? 154 : 125)
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
        visible: !control.isClockRun
        text: control.acceptedDisplay
        color: "#393b39"; font.family: (typeof studioFontFamily !== "undefined" ? studioFontFamily : "Arial")
        font.pixelSize: 12
    }
    Loader {
        id: input
        visible: !control.isClockRun
        enabled: !control.isClockRun
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
            onFocusChanged: if (!focus) input.focus = false
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
            onFocusChanged: if (!focus) input.focus = false
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
            readonly property bool isFader: control.presentation === "fader"
            width: isFader ? Math.min(72, input.width) : input.width
            height: isFader ? input.height : 34
            x: isFader ? (input.width - width) / 2 : 0
            orientation: isFader ? Qt.Vertical : Qt.Horizontal
            enabled: control.hi > control.lo
            from: 0; to: 1
            onFocusChanged: if (!focus) input.focus = false
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
                x: range.isFader ? range.width / 2 - width / 2 : range.leftPadding
                y: range.isFader ? range.topPadding : range.topPadding + range.availableHeight / 2 - height / 2
                width: range.isFader ? 3 : range.availableWidth
                height: range.isFader ? range.availableHeight : 3
                radius: 2; color: "#e4e1da"
                Rectangle {
                    x: 0
                    y: range.isFader ? parent.height * (1 - range.visualPosition) : 0
                    width: range.isFader ? parent.width : range.visualPosition * parent.width
                    height: range.isFader ? range.visualPosition * parent.height : parent.height
                    radius: 2; color: "#c5774c"
                }
            }
            handle: Rectangle {
                x: range.isFader ? (range.width - width) / 2 :
                   range.leftPadding + range.visualPosition * (range.availableWidth - width)
                y: range.isFader ? range.topPadding + (1 - range.visualPosition) * (range.availableHeight - height) :
                   range.topPadding + range.availableHeight / 2 - height / 2
                width: range.isFader ? 31 : 18; height: range.isFader ? 16 : 24
                radius: 5; color: "#393b39"
                border.color: range.activeFocus ? "#c5774c" : "#393b39"
                Rectangle {
                    anchors.centerIn: parent
                    width: range.isFader ? 12 : 2; height: range.isFader ? 2 : 10
                    color: "#faf9f6"; radius: 1
                }
            }
            ToolTip.visible: hovered
            ToolTip.text: control.descriptor.name + " · " + control.lo + "–" + control.hi + " " + control.descriptor.unit
        }
    }
    StudioButton {
        id: clockRunButton
        objectName: "clockRunToggle"
        visible: control.isClockRun
        enabled: control.isClockRun && control.hi > control.lo
        x: 0; y: 20; width: parent.width; height: 34
        quiet: true
        text: control.acceptedValue >= 0.5 ? "Stop Clock" : "Start Clock"
        Accessible.name: text
        ToolTip.visible: hovered
        ToolTip.text: "Toggle this Clock module only; global audio is unchanged"
        onClicked: studio.setParameter(control.nodeId, control.descriptor.id, control.acceptedValue >= 0.5 ? 0 : 1)
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
            if (input.item && input.item.objectName === "parameterStepper")
                input.item.value = Math.round(value);
        }
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#e9e7e1" }
}
