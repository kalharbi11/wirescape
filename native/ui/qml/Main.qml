import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import QtQuick.Dialogs

ApplicationWindow {
    id: root
    objectName: "studioRoot"
    width: 1400; height: 900
    minimumWidth: 1000; minimumHeight: 650
    visible: true
    title: "WIRESCAPE / STUDIO — " + studio.patchName + (studio.dirty ? " *" : "")
    color: "#eeede9"
    font.family: (typeof studioFontFamily !== "undefined" ? studioFontFamily : "Arial")
    readonly property int visibleModuleCount: studio.modules.length
    readonly property int visibleCableCount: studio.connections.length
    property int selectedNode: -1
    property int selectedCable: -1
    property int pendingNode: -1
    property string pendingPort: ""
    property string pendingKind: ""
    property string localHint: ""
    property int geometryRevision: 0
    property real zoom: 0.85
    property real panX: 38
    property real panY: 70
    property string pendingAction: ""
    property bool closeApproved: false
    property string saveContinuation: ""
    property var heldKeys: ({})
    readonly property int heldKeyCount: Object.keys(heldKeys).length
    function releaseHeldKeys() {
        let keys = heldKeys; heldKeys = ({});
        for (let key in keys) {
            if (Number(key) === Qt.Key_Space) studio.setGate(false);
            else studio.note(keys[key], false);
        }
    }
    onActiveFocusItemChanged: if (activeFocusItem !== keyboardSurface) releaseHeldKeys()
    // Window-scoped so Escape exits fullscreen even when a toolbar/control has focus.
    Shortcut {
        sequence: "Esc"
        context: Qt.WindowShortcut
        enabled: root.visibility === Window.FullScreen
        onActivated: root.visibility = Window.Windowed
    }
    Connections {
        target: studio
        function onStatusChanged() { if (!studio.running) root.releaseHeldKeys(); }
    }
    function beginSave(continuation) {
        releaseHeldKeys();
        saveContinuation = continuation || "";
        pendingAction = "";
        saveDialog.open();
    }
    function requestAction(action) {
        releaseHeldKeys();
        pendingAction = action;
        if (studio.dirty) unsavedDialog.open(); else finishAction();
    }
    function saveAndContinue(url) {
        // Consume the dialog transaction even when persistence fails.
        let action = saveContinuation; saveContinuation = ""; pendingAction = "";
        if (!studio.savePatch(url)) { saveFailureDialog.open(); return false; }
        if (action) { pendingAction = action; finishAction(); }
        return true;
    }
    function finishAction() {
        let action = pendingAction; pendingAction = "";
        if (action === "new") studio.resetPatch();
        else if (action === "open") loadDialog.open();
        else if (action === "close") { closeApproved = true; root.close(); }
    }
    onClosing: function(close) {
        if (!closeApproved && studio.dirty) { close.accepted = false; requestAction("close"); }
    }
    Dialog {
        id: unsavedDialog; objectName: "unsavedDialog"
        title: "Save changes to this patch?"; modal: true; focus: true; anchors.centerIn: parent
        standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel
        onAccepted: root.beginSave(root.pendingAction)
        onDiscarded: root.finishAction()
        onRejected: root.pendingAction = ""
        Label { text: "Your patch has unsaved changes." }
    }
    Dialog {
        id: saveFailureDialog; objectName: "saveFailureDialog"
        title: "Patch was not saved"; modal: true; focus: true; anchors.centerIn: parent
        standardButtons: Dialog.Retry | Dialog.Cancel
        onAccepted: root.beginSave("")
        Label {
            width: 330; wrapMode: Text.Wrap
            text: "Your edits are still here. The requested New, Open or Close was cancelled. Retry saves the current patch only.\n\n" + studio.errorText
        }
    }
    function takeKeyboardFocus() {
        // A FocusScope remembers its focused child; explicitly leave that control.
        if (activeFocusItem && activeFocusItem !== keyboardSurface) activeFocusItem.focus = false;
        keyboardSurface.forceActiveFocus();
    }
    function selectNode(id) { selectedNode = id; selectedCable = -1; }
    function chooseOutput(id, port, kind) {
        if (studio.running) { localHint = "Stop audio to edit connections."; return; }
        pendingNode = id; pendingPort = port; pendingKind = kind;
        localHint = "Choose a " + kind + " input to complete the cable. Esc cancels.";
    }
    function chooseInput(id, port, kind) {
        if (pendingNode < 0) { localHint = "Choose an output first, then an input."; return; }
        if (kind !== pendingKind) { localHint = "This input needs a " + kind + " signal."; return; }
        studio.connectPorts(pendingNode, pendingPort, id, port);
        pendingNode = -1; localHint = "";
    }
    function centerNode(card) {
        panX = viewport.width / 2 - (card.x + card.width / 2) * zoom;
        panY = viewport.height / 2 - (card.y + card.height / 2) * zoom;
    }
    function endpoint(nodeId, portId, output) {
        let revision = geometryRevision;
        for (let i = 0; i < cards.count; ++i) {
            let card = cards.itemAt(i);
            if (!card || card.descriptor.id !== nodeId) continue;
            let ports = output ? card.descriptor.outputs : card.descriptor.inputs;
            for (let j = 0; j < ports.length; ++j)
                if (ports[j].id === portId) return Qt.point(card.x + (output ? card.width : 0), card.y + 112 + j * 24);
        }
        return Qt.point(0, 0);
    }
    function zoomAt(factor, x, y) {
        let old = zoom;
        zoom = Math.max(0.35, Math.min(1.7, zoom * factor));
        panX = x - (x - panX) * zoom / old;
        panY = y - (y - panY) * zoom / old;
    }
    function fitPatch() {
        if (!cards.count) { panX = 38; panY = 70; zoom = 0.85; return; }
        let left = Infinity, top = Infinity, right = -Infinity, bottom = -Infinity;
        for (let i = 0; i < cards.count; ++i) {
            let card = cards.itemAt(i); if (!card) continue;
            left = Math.min(left, card.x); top = Math.min(top, card.y);
            right = Math.max(right, card.x + card.width); bottom = Math.max(bottom, card.y + card.height);
        }
        zoom = Math.max(0.35, Math.min(1, Math.min((viewport.width - 90) / (right - left), (viewport.height - 110) / (bottom - top))));
        panX = (viewport.width - (right - left) * zoom) / 2 - left * zoom;
        panY = (viewport.height - (bottom - top) * zoom) / 2 - top * zoom;
    }
    function keyNote(key) {
        let keys = [Qt.Key_A, Qt.Key_W, Qt.Key_S, Qt.Key_E, Qt.Key_D, Qt.Key_F, Qt.Key_T, Qt.Key_G, Qt.Key_Y, Qt.Key_H, Qt.Key_U, Qt.Key_J, Qt.Key_K];
        let index = keys.indexOf(key); return index < 0 ? -1 : 60 + index;
    }
    onActiveChanged: if (!active) { releaseHeldKeys(); studio.setGate(false); studio.panic(); }
    FileDialog {
        id: saveDialog; objectName: "saveDialog"; title: "Save patch"; fileMode: FileDialog.SaveFile
        nameFilters: ["Wirescape patch (*.json)"]; defaultSuffix: "json"
        onAccepted: root.saveAndContinue(selectedFile)
        onRejected: { root.pendingAction = ""; root.saveContinuation = ""; }
    }
    FileDialog {
        id: loadDialog; title: "Open patch"; fileMode: FileDialog.OpenFile
        nameFilters: ["Wirescape patch (*.json)", "All files (*)"]
        onAccepted: studio.loadPatch(selectedFile)
    }
    FileDialog {
        id: renderDialog; title: "Test render / 4 s mono / 2 s gate / gain 0.2"; fileMode: FileDialog.SaveFile
        nameFilters: ["Wave audio (*.wav)"]; defaultSuffix: "wav"
        onAccepted: studio.renderDemo(selectedFile)
    }
    FocusScope {
        id: keyboardSurface
        objectName: "keyboardSurface"
        anchors.fill: parent; focus: true
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Escape) { root.pendingNode = -1; root.localHint = ""; event.accepted = true; return; }
            if (root.activeFocusItem !== keyboardSurface || event.isAutoRepeat) return;
            if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) return;
            if (root.heldKeys[event.key] !== undefined) { event.accepted = true; return; }
            let n = root.keyNote(event.key);
            if (event.key === Qt.Key_Space || n >= 0) {
                let keys = Object.assign({}, root.heldKeys); keys[event.key] = n; root.heldKeys = keys;
                if (event.key === Qt.Key_Space) studio.setGate(true); else studio.note(n, true);
                event.accepted = true;
            }
        }
        Keys.onReleased: function(event) {
            if (event.isAutoRepeat || root.heldKeys[event.key] === undefined) return;
            let n = root.heldKeys[event.key];
            let keys = Object.assign({}, root.heldKeys); delete keys[event.key]; root.heldKeys = keys;
            if (event.key === Qt.Key_Space) studio.setGate(false); else studio.note(n, false);
            event.accepted = true;
        }
        Rectangle {
            id: sidebar
            width: 202; anchors.top: parent.top; anchors.bottom: parent.bottom
            color: "#f5f4f0"
            Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: "#d9d8d2" }
            Text { x: 22; y: 29; text: "WIRESCAPE"; font.pixelSize: 25; font.letterSpacing: -1.6; color: "#393b39" }
            Text { x: 23; y: 66; text: "STUDIO / 01"; font.pixelSize: 9; font.letterSpacing: 1.8; color: "#858681" }
            Rectangle { y: 105; width: parent.width; height: 1; color: "#d9d8d2" }
            Text { x: 22; y: 133; text: "MODULE LIBRARY"; font.pixelSize: 10; font.letterSpacing: 1; color: "#393b39" }
            Text { x: 22; y: 164; text: "ESSENTIALS"; font.pixelSize: 9; font.letterSpacing: 1.3; color: "#858681" }
            Column {
                x: 12; y: 187; width: 178; spacing: 4
                Repeater {
                    model: studio.palette
                    delegate: AbstractButton {
                        id: libraryButton
                        required property var modelData
                        required property int index
                        width: 178; height: root.height < 750 ? 50 : 62; enabled: !studio.running
                        hoverEnabled: true
                        background: Rectangle { radius: 7; color: libraryButton.hovered ? "#faf9f6" : "transparent"; border.color: libraryButton.hovered ? "#d9d8d2" : "transparent" }
                        contentItem: Item {
                            Rectangle { x: 8; y: 15; width: 31; height: 31; radius: 16; color: "transparent"; border.color: "#d9d8d2"; Text { anchors.centerIn: parent; text: ["~", "/", "x", "+"][index % 4]; color: "#777e79"; font.pixelSize: 20 } }
                            Text { x: 49; y: 15; width: 104; text: libraryButton.modelData.name; elide: Text.ElideRight; color: libraryButton.enabled ? "#393b39" : "#a7a8a1"; font.pixelSize: 11 }
                            Text { x: 49; y: 34; width: 104; text: libraryButton.modelData.description; elide: Text.ElideRight; color: "#858681"; font.pixelSize: 9 }
                            Text { anchors.right: parent.right; anchors.rightMargin: 7; y: 21; text: "+"; color: "#c5774c"; font.pixelSize: 18 }
                        }
                        onClicked: { studio.addModule(modelData.id); root.takeKeyboardFocus(); }
                        ToolTip.visible: hovered; ToolTip.text: modelData.description
                    }
                }
            }
            Column {
                x: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: 25; width: 158; spacing: 13
                Rectangle { width: parent.width; height: 1; color: "#d9d8d2" }
                Text { text: "SIGNAL TYPES"; font.pixelSize: 9; font.letterSpacing: 1; color: "#858681" }
                Repeater {
                    model: [{name: "Audio", tone: "#777e79"}, {name: "Control / gate", tone: "#c17853"}]
                    delegate: Row { required property var modelData; spacing: 10; Rectangle { width: 20; height: 2; y: 6; color: modelData.tone } Text { text: modelData.name; color: "#858681"; font.pixelSize: 10 } }
                }
                Text { visible: root.height >= 750; width: parent.width; text: "Connect an output to an input.\nDrag the canvas to explore.\nScroll to zoom."; wrapMode: Text.Wrap; lineHeight: 1.5; font.pixelSize: 10; color: "#858681" }
                Text { width: parent.width; text: "WIRESCAPE ESSENTIALS"; font.pixelSize: 8; font.letterSpacing: 0.8; color: "#a7a8a1" }
            }
        }
        Rectangle {
            id: toolbar
            anchors.left: sidebar.right; anchors.right: parent.right; height: 75
            color: "#f5f4f0"
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#d9d8d2" }
            Column { x: 25; width: Math.max(0, toolbar.width - toolbarActions.width - 65); anchors.verticalCenter: parent.verticalCenter; spacing: 7
                Text { width: parent.width; elide: Text.ElideRight; text: "WORKSPACE  /  " + studio.patchName + (studio.dirty ? " •" : ""); color: "#393b39"; font.pixelSize: 11; font.letterSpacing: 0.5 }
                Text { width: parent.width; elide: Text.ElideRight; text: studio.running ? "Live signal · stop audio to edit the patch" : "A small system. An open signal path."; color: "#858681"; font.pixelSize: 10 }
            }
            Row { id: toolbarActions; objectName: "toolbarActions"; anchors.right: parent.right; anchors.rightMargin: 22; anchors.verticalCenter: parent.verticalCenter; spacing: 5
                StudioButton { objectName: "newButton"; text: "New"; quiet: true; enabled: !studio.running; onClicked: root.requestAction("new") }
                StudioButton { objectName: "openButton"; text: "Open"; quiet: true; enabled: !studio.running; onClicked: root.requestAction("open") }
                StudioButton { objectName: "saveButton"; text: "Save"; onClicked: root.beginSave("") }
                StudioButton { text: "Test render"; enabled: !studio.running; onClicked: { root.releaseHeldKeys(); renderDialog.open(); } }
                Item { width: 9; height: 1 }
                StudioButton { text: studio.cleanupPending ? "Retry audio cleanup" : studio.running ? "Stop audio" : "Start audio"; accent: true; enabled: (studio.running || studio.cleanupPending || studio.ready); objectName: "audioButton"; onClicked: { root.releaseHeldKeys(); (studio.running || studio.cleanupPending) ? studio.stopAudio() : studio.startAudio(); } }
                Item { width: 9; height: 1 }
                StudioButton {
                    objectName: "minimizeButton"; text: root.width < 1200 ? "Min" : "Minimize"; quiet: true
                    Accessible.name: "Minimize window"
                    onClicked: root.visibility = Window.Minimized
                    ToolTip.visible: hovered; ToolTip.text: "Minimize to the taskbar"
                }
                StudioButton {
                    objectName: "fullscreenButton"; quiet: true
                    text: root.width < 1200 ? (root.visibility === Window.FullScreen ? "Exit" : "Full") :
                          (root.visibility === Window.FullScreen ? "Exit full screen" : "Full screen")
                    Accessible.name: root.visibility === Window.FullScreen ? "Exit full screen" : "Full screen"
                    onClicked: root.visibility = root.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen
                    ToolTip.visible: hovered; ToolTip.text: "Toggle full screen (Esc to exit)"
                }
            }
        }
        Item {
            id: viewport
            anchors.left: sidebar.right; anchors.right: parent.right; anchors.top: toolbar.bottom; anchors.bottom: transport.top
            clip: true
            Rectangle { anchors.fill: parent; color: "#eeede9" }
            Repeater {
                model: Math.ceil(viewport.width / 32) + 1
                delegate: Rectangle { required property int index; x: index * 32 + root.panX % 32; width: 1; height: viewport.height; color: "#e6e4de"; opacity: 0.6 }
            }
            Repeater {
                model: Math.ceil(viewport.height / 32) + 1
                delegate: Rectangle { required property int index; y: index * 32 + root.panY % 32; height: 1; width: viewport.width; color: "#e6e4de"; opacity: 0.6 }
            }
            MouseArea {
                anchors.fill: parent
                property real startX: 0; property real startY: 0
                property real originalX: 0; property real originalY: 0
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                onPressed: function(mouse) { startX = mouse.x; startY = mouse.y; originalX = root.panX; originalY = root.panY; root.takeKeyboardFocus(); root.selectedNode = -1; }
                onPositionChanged: function(mouse) { if (pressed) { root.panX = originalX + mouse.x - startX; root.panY = originalY + mouse.y - startY; } }
                onWheel: function(wheel) { root.zoomAt(wheel.angleDelta.y > 0 ? 1.1 : 1 / 1.1, wheel.x, wheel.y); wheel.accepted = true; }
            }
            Item {
                id: world
                x: root.panX; y: root.panY
                width: 1; height: 1; scale: root.zoom; transformOrigin: Item.TopLeft
                Repeater {
                    id: cables
                    model: studio.connections
                    delegate: Shape {
                        id: cable
                        required property var modelData
                        required property int index
                        property point a: root.endpoint(modelData.fromNode, modelData.fromPort, true)
                        property point b: root.endpoint(modelData.toNode, modelData.toPort, false)
                        property real bend: Math.max(55, Math.abs(b.x - a.x) * 0.45)
                        width: 1; height: 1
                        preferredRendererType: Shape.CurveRenderer
                        ShapePath {
                            strokeColor: root.selectedCable === cable.index ? "#c5774c" : cable.modelData.kind === "audio" ? "#777e79" : "#c17853"
                            strokeWidth: root.selectedCable === cable.index ? 3.2 : 2.2
                            fillColor: "transparent"
                            startX: cable.a.x; startY: cable.a.y
                            PathCubic { x: cable.b.x; y: cable.b.y; control1X: cable.a.x + cable.bend; control1Y: cable.a.y + 20; control2X: cable.b.x - cable.bend; control2Y: cable.b.y + 20 }
                        }
                        Rectangle {
                            visible: cable.modelData.channels > 1
                            x: (cable.a.x + cable.b.x) / 2 + 10
                            y: (cable.a.y + cable.b.y) / 2 + 8
                            width: 22; height: 17; radius: 4; color: "#eeede9"
                            Text { anchors.centerIn: parent; text: cable.modelData.channels; font.pixelSize: 11; color: "#614d42" }
                        }
                        Rectangle {
                            x: (cable.a.x + cable.b.x) / 2 - 7
                            y: (cable.a.y + cable.b.y) / 2 + 15 - 7
                            width: 14; height: 14; radius: 7
                            color: "#eeede9"
                            border.width: 2
                            border.color: root.selectedCable === cable.index ? "#c5774c" : cable.modelData.kind === "audio" ? "#777e79" : "#c17853"
                            MouseArea {
                                anchors.fill: parent; anchors.margins: -7
                                cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                onClicked: { root.selectedCable = cable.index; root.selectedNode = -1; }
                                ToolTip.visible: containsMouse
                                ToolTip.text: "Select cable"
                            }
                        }
                    }
                }
                Repeater {
                    id: cards
                    model: studio.modules
                    onItemAdded: root.geometryRevision++
                    onItemRemoved: root.geometryRevision++
                    delegate: ModuleCard { required property var modelData; descriptor: modelData; workspace: root; selected: root.selectedNode === descriptor.id }
                }
            }
            Row {
                anchors.right: parent.right; anchors.rightMargin: 20; anchors.bottom: parent.bottom; anchors.bottomMargin: 18; spacing: 5
                StudioButton { text: "−"; onClicked: root.zoomAt(1 / 1.15, viewport.width / 2, viewport.height / 2) }
                StudioButton { text: Math.round(root.zoom * 100) + "%"; onClicked: root.fitPatch(); ToolTip.visible: hovered; ToolTip.text: "Fit all modules" }
                StudioButton { text: "+"; onClicked: root.zoomAt(1.15, viewport.width / 2, viewport.height / 2) }
            }
            Rectangle {
                visible: root.pendingNode >= 0
                x: 20; y: 18; width: pendingLabel.implicitWidth + 56; height: 34; radius: 6; color: "#faf9f6"; border.color: "#c5774c"
                Text { id: pendingLabel; x: 12; anchors.verticalCenter: parent.verticalCenter; text: "PATCHING · SELECT AN INPUT"; color: "#a7603d"; font.pixelSize: 10; font.letterSpacing: 0.5 }
                StudioButton { anchors.right: parent.right; width: 30; implicitWidth: 30; height: 32; text: "×"; quiet: true; onClicked: { root.pendingNode = -1; root.localHint = ""; } }
            }
            Row {
                visible: root.selectedCable >= 0 && root.selectedCable < studio.connections.length
                x: 20; anchors.bottom: parent.bottom; anchors.bottomMargin: 18; spacing: 5
                StudioButton { text: "Disconnect cable"; enabled: !studio.running; onClicked: { studio.disconnectCable(root.selectedCable); root.selectedCable = -1; } }
                StudioButton { text: "×"; quiet: true; onClicked: root.selectedCable = -1 }
            }
        }
        Rectangle {
            id: transport
            anchors.left: sidebar.right; anchors.right: parent.right; anchors.bottom: statusbar.top; height: root.width < 1200 ? 106 : 70
            color: "#f5f4f0"
            Rectangle { width: parent.width; height: 1; color: "#d9d8d2" }
            Row {
                objectName: "performanceControls"; x: 23; y: root.width < 1200 ? 10 : (transport.height - height) / 2; spacing: 10
                StudioButton { objectName: "gateButton"; text: "Hold gate"; onDownChanged: studio.setGate(down); ToolTip.visible: hovered; ToolTip.text: "Hold to play the envelope · Space on canvas" }
                StudioButton { objectName: "panicButton"; text: "Panic"; quiet: true; onClicked: { root.releaseHeldKeys(); studio.panic(); } }
                Column { spacing: 5; y: 2
                    Text { text: "LAST NOTE / RETRIGGER - KNOB = BASE PITCH"; font.pixelSize: 9; font.letterSpacing: 0.8; color: "#858681" }
                    Text { text: "A W S E D F T G Y H U J K"; font.pixelSize: 10; color: "#393b39" }
                }
            }
            Row {
                objectName: "deviceControls"; anchors.right: parent.right; anchors.rightMargin: 22; y: root.width < 1200 ? 62 : (transport.height - height) / 2; spacing: 13
                StudioButton { text: "Refresh"; enabled: !studio.running; onClicked: studio.refreshDevices(); ToolTip.visible: hovered; ToolTip.text: "Discover audio outputs without starting playback" }
                ComboBox {
                    id: deviceBox; objectName: "deviceBox"
                    width: root.width < 1150 ? 130 : 190; height: 32
                    model: studio.devices; textRole: "name"; enabled: !studio.running
                    currentIndex: {
                        for (let i = 0; i < studio.devices.length; ++i)
                            if (studio.devices[i].id === studio.selectedDevice) return i;
                        return 0;
                    }
                    font.pixelSize: 10
                    onActivated: function(index) { studio.setDevice(model[index].id); }
                    background: Rectangle { color: "#faf9f6"; border.color: "#d9d8d2"; radius: 5 }
                    ToolTip.visible: hovered; ToolTip.text: "Audio output device"
                }
                Column { spacing: 5
                    Text { text: "OUTPUT"; color: "#858681"; font.pixelSize: 8; font.letterSpacing: 1 }
                    Rectangle { width: 72; height: 7; radius: 2; color: "#dedcd5"; Rectangle { width: Math.min(1, Math.max(0, studio.peak)) * parent.width; height: parent.height; radius: 2; color: studio.peak > 0.95 ? "#b85d43" : "#777e79" } }
                }
            }
        }
        Rectangle {
            id: statusbar
            anchors.left: sidebar.right; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 34
            color: "#eeede9"
            Rectangle { width: parent.width; height: 1; color: "#d9d8d2" }
            Rectangle { x: 23; anchors.verticalCenter: parent.verticalCenter; width: 5; height: 5; radius: 3; color: studio.running ? "#c5774c" : "#a7a8a1" }
            Text { x: 37; anchors.verticalCenter: parent.verticalCenter; width: Math.max(0, parent.width - statusMetrics.width - 65); elide: Text.ElideRight; text: studio.errorText || studio.noticeText || root.localHint || studio.statusText; color: studio.errorText ? "#b85d43" : "#858681"; font.pixelSize: 10 }
            Text { id: statusMetrics; width: Math.min(implicitWidth, parent.width * 0.55); elide: Text.ElideLeft; anchors.right: parent.right; anchors.rightMargin: 23; anchors.verticalCenter: parent.verticalCenter; text: (studio.diagnosticText ? studio.diagnosticText + "  /  " : "") + (studio.sampleRate / 1000).toFixed(1) + " kHz  /  " + studio.blockSize + "  ·  CPU " + Number(studio.cpuLoad).toFixed(1) + "%" + (studio.underruns ? "  ·  " + studio.underruns + " dropouts" : ""); color: "#858681"; font.pixelSize: 9 }
        }
    }
}
