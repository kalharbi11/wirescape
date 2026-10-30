import QtQuick
import QtQuick.Controls

Rectangle {
    id: card
    objectName: "module_" + descriptor.id
    required property var descriptor
    required property var workspace
    property bool selected: false
    readonly property int portRows: Math.max(descriptor.inputs.length, descriptor.outputs.length)
    readonly property real controlsTop: 108 + portRows * 24
    width: 280
    height: controlsTop + parameters.height + 30
    radius: 13; color: "#faf9f6"
    border.color: selected ? "#c5774c" : "#d9d8d2"
    x: descriptor.x; y: descriptor.y
    onXChanged: workspace.geometryRevision++
    onYChanged: workspace.geometryRevision++
    Rectangle { x: 1; y: 81; width: parent.width - 2; height: 1; color: "#d9d8d2" }
    Text { x: 22; y: 15; text: "ESSENTIALS / " + String(card.descriptor.id).padStart(2, "0"); font.family: (typeof studioFontFamily !== "undefined" ? studioFontFamily : "Arial"); font.pixelSize: 9; font.letterSpacing: 1; color: "#858681" }
    Text { x: 21; y: 34; text: card.descriptor.title; font.family: (typeof studioFontFamily !== "undefined" ? studioFontFamily : "Arial"); font.pixelSize: 26; font.letterSpacing: -1.4; font.weight: Font.Medium; color: "#262824" }
    MouseArea {
        objectName: "moduleHeader_" + card.descriptor.id
        x: 0; y: 0; width: parent.width - 37; height: 81
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        drag.target: card; drag.axis: Drag.XAndYAxis
        onPressed: { workspace.selectNode(card.descriptor.id); workspace.takeKeyboardFocus(); }
        onReleased: studio.moveModule(card.descriptor.id, card.x, card.y)
        onDoubleClicked: workspace.centerNode(card)
    }
    StudioButton {
        x: parent.width - 36; y: 15; width: 27; implicitWidth: 27; height: 27; implicitHeight: 27
        text: "×"; quiet: true
        enabled: !studio.running
        onClicked: studio.removeModule(card.descriptor.id)
        ToolTip.visible: hovered; ToolTip.text: "Remove module"
    }
    Text {
        x: 22; y: 88; width: parent.width - 44; height: 14
        text: card.descriptor.missing ? "MODULE UNAVAILABLE" : card.descriptor.description
        elide: Text.ElideRight; color: card.descriptor.missing ? "#b85d43" : "#858681"; font.pixelSize: 9; font.family: (typeof studioFontFamily !== "undefined" ? studioFontFamily : "Arial")
    }
    // Map the release through the scene transform, including pan and zoom.
    function inputAt(position) {
        for (let i = 0; i < inputPorts.count; ++i) {
            let row = inputPorts.itemAt(i);
            let p = row.mapFromItem(workspace.contentItem, position.x, position.y);
            if (p.x >= -4 && p.x <= row.width + 4 && p.y >= -4 && p.y <= row.height + 4)
                return row.modelData;
        }
        return null;
    }
    Repeater {
        id: inputPorts
        model: card.descriptor.inputs
        delegate: Item {
            required property var modelData
            required property int index
            x: -8; y: 112 + index * 24 - 8; width: 135; height: 16
            Rectangle { x: 0; width: 15; height: 15; radius: 8; color: "#faf9f6"; border.width: 1; border.color: modelData.kind === "audio" ? "#393b39" : "#c5774c"; Rectangle { anchors.centerIn: parent; width: 5; height: 5; radius: 3; color: parent.border.color } }
            Text { x: 23; anchors.verticalCenter: parent.verticalCenter; text: modelData.label.toUpperCase(); color: "#858681"; font.pixelSize: 9; font.family: (typeof studioFontFamily !== "undefined" ? studioFontFamily : "Arial") }
            MouseArea { objectName: "input_" + card.descriptor.id + "_" + modelData.id; anchors.fill: parent; anchors.margins: -4; cursorShape: Qt.CrossCursor; onClicked: workspace.chooseInput(card.descriptor.id, modelData.id, modelData.kind); hoverEnabled: true; ToolTip.visible: containsMouse; ToolTip.text: modelData.label + " · " + modelData.kind + " input" }
        }
    }
    Repeater {
        model: card.descriptor.outputs
        delegate: Item {
            required property var modelData
            required property int index
            x: card.width - 127; y: 112 + index * 24 - 8; width: 135; height: 16
            Text { anchors.right: parent.right; anchors.rightMargin: 23; anchors.verticalCenter: parent.verticalCenter; text: modelData.label.toUpperCase(); color: "#858681"; font.pixelSize: 9; font.family: (typeof studioFontFamily !== "undefined" ? studioFontFamily : "Arial") }
            Rectangle { anchors.right: parent.right; width: 15; height: 15; radius: 8; color: workspace.pendingNode === card.descriptor.id && workspace.pendingPort === modelData.id ? "#c5774c" : "#faf9f6"; border.width: 1; border.color: modelData.kind === "audio" ? "#393b39" : "#c5774c"; Rectangle { anchors.centerIn: parent; width: 5; height: 5; radius: 3; color: parent.border.color } }
            MouseArea { objectName: "output_" + card.descriptor.id + "_" + modelData.id; anchors.fill: parent; anchors.margins: -4; cursorShape: Qt.CrossCursor
                property point pressPoint
                onPressed: function(mouse) {
                    pressPoint = Qt.point(mouse.x, mouse.y);
                    workspace.chooseOutput(card.descriptor.id, modelData.id, modelData.kind);
                }
                onReleased: function(mouse) {
                    if (Math.hypot(mouse.x - pressPoint.x, mouse.y - pressPoint.y) >= Qt.styleHints.startDragDistance)
                        workspace.completeCableDrag(mapToItem(workspace.contentItem, mouse.x, mouse.y));
                }
                hoverEnabled: true; ToolTip.visible: containsMouse; ToolTip.text: modelData.label + " · " + modelData.kind + " output" }
        }
    }
    // UI-only mapping of parameter IDs from native/modules/*/module.json.
    // Unknown modules and parameters deliberately retain the stock slider.
    function controlStyle(p) {
        var m = descriptor.moduleId, id = Number(p.id);
        if (m === "essentials.mixer") return "fader";
        if (m === "essentials.clock" && id === 1) return "stepper";
        if (m === "essentials.adsr") return id === 2 ? "knob" : "slider";
        if ((m === "essentials.vco" && id === 0) ||
            (m === "essentials.lfo" && id >= 0 && id <= 2) ||
            (m === "essentials.vcf" && id >= 0 && id <= 1) ||
            (m === "essentials.ladder" && id >= 0 && id <= 2) ||
            (m === "essentials.function_envelope" && id >= 0 && id <= 2) ||
            (m === "essentials.slew" && id >= 0 && id <= 1) ||
            (m === "essentials.scale_offset" && id >= 0 && id <= 1) ||
            (m === "essentials.clock" && id === 0))
            return "knob";
        return "slider";
    }
    function priority(p) {
        var m = descriptor.moduleId, id = Number(p.id);
        return id === 0 && (m === "essentials.vco" || m === "essentials.lfo" ||
               m === "essentials.vcf" || m === "essentials.ladder" ||
               m === "essentials.clock") ? "primary" : "related";
    }
    function controlWidth(p) {
        var style = controlStyle(p);
        return style === "fader" || style === "stepper" ||
               (style === "knob" && priority(p) !== "primary") ?
               (parameters.width - parameters.spacing) / 2 : parameters.width;
    }
    Flow {
        id: parameters
        x: 22; y: card.controlsTop; width: parent.width - 44
        spacing: 8
        Repeater {
            model: card.descriptor.parameters
            delegate: ParameterControl {
                required property var modelData
                width: card.controlWidth(modelData)
                descriptor: modelData
                nodeId: card.descriptor.id
                moduleId: card.descriptor.moduleId
                presentation: card.controlStyle(modelData)
                hierarchy: card.priority(modelData)
                discrete: card.descriptor.moduleId === "essentials.clock" &&
                          (Number(modelData.id) === 1 || Number(modelData.id) === 3)
            }
        }
    }
    StudioButton {
        visible: card.descriptor.moduleId === "essentials.output"
        x: 22; y: card.controlsTop; height: 25
        text: studio.outputNode === card.descriptor.id ? "SELECTED OUTPUT" : "Use this output"
        enabled: !studio.running && studio.outputNode !== card.descriptor.id
        onClicked: studio.selectOutput(card.descriptor.id)
    }
}
