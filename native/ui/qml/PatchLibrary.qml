// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtCore

// Read-only starters; recent/favourite entries store URLs, never patch contents.
Item {
    id: library
    objectName: "patchLibraryPanel"
    width: 218; height: 34
    property string currentFile: ""
    property bool running: false
    // Tests use transient history so they never modify user application settings.
    // The offscreen test application has a different name, so do not even
    // construct QSettings there (or touch real users' history).
    property bool persistHistory: Qt.application.name === "Wirescape"
    property string sessionRecent: "[]"
    property string sessionFavourites: "[]"
    signal openRequested(string url)
    Loader {
        id: saved
        active: library.persistHistory
        sourceComponent: Component {
            Settings {
                category: "PatchLibrary"
                property string recentPaths: "[]"
                property string favouritePaths: "[]"
            }
        }
    }
    readonly property var starters: [
        // QUrl's file: relative-path form round-trips to the Qt resource
        // filename ":/starter/..." through StudioController's toLocalFile().
        {label: "Minimal synth", path: "file::/starter/starter_minimal.json",
         hint: "Continuous VCO tone · no MIDI needed"},
        {label: "LFO / filter / VCA", path: "file::/starter/starter_lfo_filter.json",
         hint: "Filtered tone pulses with LFO amplitude · no MIDI"},
        {label: "MIDI-controlled synth", path: "file::/starter/starter_midi.json",
         hint: "Continuous reference tone; MIDI input changes pitch · enable an endpoint"},
        {label: "Poly chords", path: "file::/starter/starter_poly.json",
         hint: "Four-voice keyboard pitch · play A–K keys"},
        {label: "CHAOS-lite", path: "file::/starter/starter_chaos.json",
         hint: "Layered CHAOS oscillator · 48 kHz, no MIDI"},
        {label: "Macro Oscillator", path: "file::/starter/starter_macro.json",
         hint: "Macro main output · 48 kHz; play A–K keys to trigger"}
    ]
    function paths(text) {
        try {
            var data = JSON.parse(text);
            return Array.isArray(data) ? data.filter(isUserFile) : [];
        } catch (error) { return []; }
    }
    function isUserFile(path) {
        return typeof path === "string" && path.indexOf("file:/") === 0;
    }
    function filename(path) {
        try { return decodeURIComponent(path.substring(path.lastIndexOf("/") + 1)); }
        catch (error) { return path.substring(path.lastIndexOf("/") + 1); }
    }
    function buildEntries(recent, favourites) {
        var choices = [{label: "Patches…", path: "", hint: "Open a starter or saved patch"}];
        for (var i = 0; i < starters.length; ++i) choices.push(starters[i]);
        var favouritesList = paths(favourites), recentList = paths(recent);
        for (var j = 0; j < favouritesList.length; ++j)
            choices.push({label: "★ " + filename(favouritesList[j]), path: favouritesList[j],
                          hint: "Favourite saved patch · read-only open"});
        for (var k = 0; k < recentList.length; ++k)
            if (favouritesList.indexOf(recentList[k]) < 0)
                choices.push({label: "Recent: " + filename(recentList[k]), path: recentList[k],
                              hint: "Recently opened saved patch"});
        return choices;
    }
    readonly property var entries: buildEntries(
        persistHistory && saved.item ? saved.item.recentPaths : sessionRecent,
        persistHistory && saved.item ? saved.item.favouritePaths : sessionFavourites)
    readonly property int entryCount: entries.length
    function recordOpened(url) {
        url = String(url);
        if (!isUserFile(url)) return;
        var current = paths(persistHistory && saved.item ? saved.item.recentPaths : sessionRecent)
            .filter(function(item) { return item !== url; });
        current.unshift(url);
        if (persistHistory && saved.item) saved.item.recentPaths = JSON.stringify(current.slice(0, 12));
        else sessionRecent = JSON.stringify(current.slice(0, 12));
    }
    function isFavourite(url) {
        return paths(persistHistory && saved.item ? saved.item.favouritePaths : sessionFavourites)
            .indexOf(String(url)) >= 0;
    }
    function toggleFavourite() {
        if (!isUserFile(currentFile) || running) return;
        var current = paths(persistHistory && saved.item ? saved.item.favouritePaths : sessionFavourites);
        var index = current.indexOf(currentFile);
        if (index >= 0) current.splice(index, 1);
        else current.unshift(currentFile);
        if (persistHistory && saved.item) saved.item.favouritePaths = JSON.stringify(current.slice(0, 12));
        else sessionFavourites = JSON.stringify(current.slice(0, 12));
    }
    ComboBox {
        id: selector
        objectName: "patchLibraryDropdown"
        width: parent.width - 38; height: 34
        enabled: !library.running
        model: library.entries; textRole: "label"
        currentIndex: 0
        font.pixelSize: 10
        onActivated: function(index) {
            var selected = library.entries[index];
            if (selected && selected.path) library.openRequested(selected.path);
            currentIndex = 0;
        }
        delegate: ItemDelegate {
            width: selector.width
            text: modelData.label
            font.pixelSize: 10
            ToolTip.visible: hovered
            ToolTip.text: modelData.hint
        }
        ToolTip.visible: hovered
        ToolTip.text: "Starter patches, favourites, and recently opened files"
    }
    StudioButton {
        objectName: "favouritePatchButton"
        x: parent.width - 36; width: 36; implicitWidth: 36; height: 34
        text: library.isFavourite(library.currentFile) ? "★" : "☆"
        quiet: true; enabled: !library.running && library.isUserFile(library.currentFile)
        onClicked: library.toggleFavourite()
        ToolTip.visible: hovered
        ToolTip.text: "Toggle favourite for the currently opened saved patch"
    }
}
