"""CHAOS — temporary engineering control panel (Dear PyGui).

Run:  python ui.py            (needs the surgepy module; see README)
      python ui.py --no-audio (renders silently, for testing the UI without a sound device)

Keys: Z S X D C V G B H N J M ,  = one octave (C..C) · UP/DOWN = octave · SPACE = panic
"""
import glob
import json
import os
import sys
import threading
import time

import numpy as np
import dearpygui.dearpygui as dpg

import analysis as A
import engine as E
import macros as M
import voices as V
from modlab import GROUPS, INTENSITY, Randomizer, SHAPES, TARGETS, MATRIX_SOURCES, MATRIX_TARGETS

HERE = os.path.dirname(os.path.abspath(__file__))
PRESETS = os.path.join(HERE, "presets")
W, H = 1280, 820
SG_W, SG_H = 300, 160  # spectrogram texture size

BLACK, WHITE, GRAY, LIGHT = (0, 0, 0, 255), (255, 255, 255, 255), (150, 150, 150, 255), (228, 228, 228, 255)

LAYOUT = [
    ("SCREAM", [("BREAK", "%.2f"), ("DRIVE_TYPE", None), ("ROUGH_RATE", "%.0f Hz"), ("ROUGH_DEPTH", "%.2f"),
                ("JITTER", "%.2f"), ("SUB", "%.2f"), ("FM_RATIO", "%.2f")]),
    ("UNSTABLE", [("DETUNE", "%.1f c"), ("VOICES", "%d"), ("DRIFT", "%.2f"), ("OFFSET", "%+.0f c"),
                  ("CLUSTER", "%d"), ("QUARTER", "%d"), ("FALL", "%.2f"), ("CUTOFF", "%.2f")]),
    ("DECAY", [("TAPE", "%.2f"), ("SPACE", "%.2f"), ("SPACE_MIX", "%.2f")]),
    ("LOOP / MOD", [("LOOP_SPEED", "%+.2f x"), ("LOOP_FEEDBACK", "%.2f"), ("LOOP_MIX", "%.2f"),
                    ("LFO_RATE", "%.2f Hz"), ("LFO_DEPTH", "%.2f"), ("LFO_SELF", "%.2f")]),
    ("OUT", [("MASTER_DB", "%.0f dB")]),
]

KEYMAP = ["Z", "S", "X", "D", "C", "V", "G", "B", "H", "N", "J", "M", "Comma"]


class App:
    def __init__(self, audio=True):
        self.eng = E.Engine()
        self.ui_state = dict(self.eng.state)
        self.rand = Randomizer(M.LIMITS)
        self.octave = 4
        self.down = set()
        self.hold = False
        self.stream = None
        self.audio = audio
        self.sg = np.ones((SG_H, SG_W, 4), np.float32)
        self.peak_hold = None
        self.lfo_shape, self.lfo_target = "smooth", "off"

    # ------------------------------------------------------------------ audio
    def start_audio(self):
        if self.audio:
            try:
                import sounddevice as sd
                fifo = np.zeros((2, 0), np.float32)

                def cb(outdata, frames, t, status):
                    nonlocal fifo
                    while fifo.shape[1] < frames:
                        fifo = np.concatenate([fifo, self.eng.render(E.BLOCK)], axis=1)
                    outdata[:] = fifo[:, :frames].T
                    fifo = fifo[:, frames:]

                self.stream = sd.OutputStream(samplerate=E.SR, blocksize=E.BLOCK, channels=2,
                                              dtype="float32", callback=cb, latency="low")
                self.stream.start()
                return
            except Exception as ex:  # no device: fall through to silent mode
                print("audio unavailable, running silent:", ex)
        def loop():
            while True:
                t0 = time.perf_counter()
                self.eng.render(E.BLOCK)
                time.sleep(max(0.0, E.BLOCK / E.SR - (time.perf_counter() - t0)))
        threading.Thread(target=loop, daemon=True).start()

    # ------------------------------------------------------------------ helpers
    def set_macro(self, name, value):
        self.ui_state[name] = M.clamp(name, value)
        self.eng.set(name, value)

    def push_all(self):
        d = dict(self.ui_state)
        d["LFO_SHAPE"], d["LFO_TARGET"] = self.lfo_shape, self.lfo_target
        self.eng.set_many(d)
        for k, v in self.ui_state.items():
            if dpg.does_item_exist(f"s_{k}") and k != "QUARTER":
                dpg.set_value(f"s_{k}", int(v) if k in M.INT_MACROS else float(v))
        dpg.set_value("s_QUARTER", bool(self.ui_state["QUARTER"]))
        dpg.set_value("drive_combo", M.DRIVE_TYPES[int(self.ui_state["DRIVE_TYPE"])][0])
        dpg.set_value("lfo_shape", self.lfo_shape)
        dpg.set_value("lfo_target", self.lfo_target)
        for i in range(1, 5):
            for typ, choices in (("SRC", MATRIX_SOURCES), ("DST", MATRIX_TARGETS)):
                key = f"MOD{i}_{typ}"
                tag = f"route_{key}"
                if dpg.does_item_exist(tag):
                    dpg.set_value(tag, choices[int(self.ui_state[key])])

    def note(self, idx, on):
        n = 12 * (self.octave + 1) + idx
        if on:
            if self.hold and n in self.eng.held:
                self.eng.note_off(n)
            else:
                self.eng.note_on(n)
        elif not self.hold:
            self.eng.note_off(n)

    # ------------------------------------------------------------------ presets
    def preset_names(self):
        return sorted(os.path.splitext(os.path.basename(p))[0] for p in glob.glob(os.path.join(PRESETS, "*.json")))

    def load_preset(self, name):
        with open(os.path.join(PRESETS, name + ".json")) as f:
            d = json.load(f)
        st = dict(M.DEFAULTS)
        st.update({k: v for k, v in d.get("macros", {}).items() if k in st})
        self.ui_state = st
        self.lfo_shape = d.get("lfo_shape", "smooth")
        self.lfo_target = d.get("lfo_target", "off")
        self.push_all()
        if d.get("b_type"):
            self.eng.q.put(("btype", d["b_type"], None))
        for v, kv in d.get("raw", {}).items():
            for k, x in kv.items():
                self.eng.q.put(("raw", v, (k, x)))
                if dpg.does_item_exist(f"raw_{v}_{k}"):
                    dpg.set_value(f"raw_{v}_{k}", x)
        dpg.set_value("status", f"preset: {name} — {d.get('note', '')}")
        if d.get("hold_notes"):
            self.eng.panic()
            self.hold = True
            dpg.set_value("hold", True)
            for n in d["hold_notes"]:
                self.eng.note_on(n)

    def keep_preset(self):
        os.makedirs(PRESETS, exist_ok=True)
        name = time.strftime("kept_%Y%m%d_%H%M%S")
        with open(os.path.join(PRESETS, name + ".json"), "w") as f:
            raw = {v: {k: dpg.get_value(f"raw_{v}_{k}") for k in self.eng.raw[v] if dpg.does_item_exist(f"raw_{v}_{k}")}
                   for v in ("A", "B")}
            json.dump({"note": "saved with KEEP", "macros": self.ui_state, "raw": raw, "b_type": self.eng.b_type,
                       "lfo_shape": self.lfo_shape,
                       "lfo_target": self.lfo_target}, f, indent=1)
        dpg.configure_item("preset_list", items=self.preset_names())
        dpg.set_value("status", f"kept -> presets/{name}.json")

    def randomize(self, mode):
        g, i = dpg.get_value("rand_group"), dpg.get_value("rand_int")
        if mode == "roll":
            self.ui_state = self.rand.roll(self.ui_state, g, i)
        elif mode == "repeat":
            self.ui_state = self.rand.repeat(self.ui_state)
        else:
            self.ui_state = self.rand.undo(self.ui_state)
        self.ui_state = {k: M.clamp(k, v) for k, v in self.ui_state.items()}
        self.push_all()
        dpg.set_value("status", f"randomize {mode}: {g} {i}  (undo depth {len(self.rand.history)})")

    # ------------------------------------------------------------------ build UI
    def theme(self):
        with dpg.theme() as t:
            with dpg.theme_component(dpg.mvAll):
                for col, c in [(dpg.mvThemeCol_WindowBg, WHITE), (dpg.mvThemeCol_ChildBg, WHITE),
                               (dpg.mvThemeCol_PopupBg, WHITE), (dpg.mvThemeCol_Text, BLACK),
                               (dpg.mvThemeCol_TextDisabled, GRAY), (dpg.mvThemeCol_Border, BLACK),
                               (dpg.mvThemeCol_FrameBg, WHITE), (dpg.mvThemeCol_FrameBgHovered, LIGHT),
                               (dpg.mvThemeCol_FrameBgActive, LIGHT), (dpg.mvThemeCol_SliderGrab, BLACK),
                               (dpg.mvThemeCol_SliderGrabActive, BLACK), (dpg.mvThemeCol_Button, WHITE),
                               (dpg.mvThemeCol_ButtonHovered, LIGHT), (dpg.mvThemeCol_ButtonActive, BLACK),
                               (dpg.mvThemeCol_CheckMark, BLACK), (dpg.mvThemeCol_Header, LIGHT),
                               (dpg.mvThemeCol_HeaderHovered, LIGHT), (dpg.mvThemeCol_Separator, BLACK),
                               (dpg.mvThemeCol_TitleBg, WHITE), (dpg.mvThemeCol_TitleBgActive, WHITE),
                               (dpg.mvThemeCol_ScrollbarBg, WHITE), (dpg.mvThemeCol_ScrollbarGrab, GRAY)] + [(getattr(dpg, n), c) for n, c in [
                                   ("mvThemeCol_Tab", WHITE), ("mvThemeCol_TabHovered", LIGHT),
                                   ("mvThemeCol_TabActive", GRAY), ("mvThemeCol_TabSelected", GRAY),
                                   ("mvThemeCol_TabUnfocused", WHITE), ("mvThemeCol_TabUnfocusedActive", GRAY),
                                   ("mvThemeCol_TabSelectedOverline", BLACK)] if hasattr(dpg, n)]:
                    dpg.add_theme_color(col, c)
                for sv, v in [(dpg.mvStyleVar_FrameBorderSize, 1), (dpg.mvStyleVar_WindowBorderSize, 0),
                              (dpg.mvStyleVar_ChildBorderSize, 1), (dpg.mvStyleVar_FrameRounding, 0),
                              (dpg.mvStyleVar_GrabRounding, 7), (dpg.mvStyleVar_GrabMinSize, 12)]:
                    dpg.add_theme_style(sv, v)
                dpg.add_theme_style(dpg.mvStyleVar_FramePadding, 4, 2)
                dpg.add_theme_style(dpg.mvStyleVar_ItemSpacing, 6, 4)
            with dpg.theme_component(dpg.mvPlot):
                for col, c in [(dpg.mvPlotCol_PlotBg, WHITE), (dpg.mvPlotCol_FrameBg, WHITE),
                               (dpg.mvPlotCol_PlotBorder, BLACK), (dpg.mvPlotCol_AxisGrid, LIGHT),
                               (dpg.mvPlotCol_AxisText, BLACK), (dpg.mvPlotCol_TitleText, BLACK),
                               (dpg.mvPlotCol_Line, BLACK), (dpg.mvPlotCol_Fill, (0, 0, 0, 40))]:
                    dpg.add_theme_color(col, c, category=dpg.mvThemeCat_Plots)
        dpg.bind_theme(t)
        with dpg.theme() as self.gray_line:
            with dpg.theme_component(dpg.mvLineSeries):
                dpg.add_theme_color(dpg.mvPlotCol_Line, GRAY, category=dpg.mvThemeCat_Plots)
        with dpg.theme() as self.lane_fill:
            with dpg.theme_component(dpg.mvShadeSeries):
                dpg.add_theme_color(dpg.mvPlotCol_Fill, (0, 0, 0, 150), category=dpg.mvThemeCat_Plots)
        with dpg.theme() as self.band_fill:
            with dpg.theme_component(dpg.mvShadeSeries):
                dpg.add_theme_color(dpg.mvPlotCol_Fill, (0, 0, 0, 30), category=dpg.mvThemeCat_Plots)

    def font(self):
        for path in [os.path.join(HERE, "fonts", "JetBrainsMono-Regular.ttf"), "<drive>/Windows/Fonts/consola.ttf",
                     "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", "/System/Library/Fonts/Menlo.ttc"]:
            if os.path.exists(path):
                with dpg.font_registry():
                    f = dpg.add_font(path, 14)
                dpg.bind_font(f)
                return

    def plot(self, tag, title, xlabel, ylabel, xlog=False, ylim=None, xlim=None, h=285):
        with dpg.plot(label=title, height=h, width=-1, tag=tag, no_menus=True, no_box_select=True,
                      no_mouse_pos=True):
            xa = dpg.add_plot_axis(dpg.mvXAxis, label=xlabel, tag=tag + "_x",
                                   scale=dpg.mvPlotScale_Log10 if xlog else dpg.mvPlotScale_Linear)
            ya = dpg.add_plot_axis(dpg.mvYAxis, label=ylabel, tag=tag + "_y")
            if ylim:
                dpg.set_axis_limits(ya, *ylim)
            if xlim:
                dpg.set_axis_limits(xa, *xlim)
        return ya

    def build(self):
        dpg.create_context()
        self.font()
        self.theme()
        with dpg.texture_registry():
            dpg.add_dynamic_texture(SG_W, SG_H, self.sg.ravel(), tag="sg_tex")
        with dpg.window(tag="main"):
            with dpg.group(horizontal=True):
                dpg.add_text("CHAOS  ·  a complex synth voice  ·  engine prototype 0.1   ", color=BLACK)
            dpg.add_text("", tag="status", color=GRAY)
            dpg.add_separator()
            with dpg.tab_bar(tag="tabs"):
              with dpg.tab(label="SYNTH", tag="tab_synth"):
                with dpg.group(horizontal=True):
                    # ---- left: macros
                    with dpg.child_window(width=400, height=-60, border=False):
                        with dpg.tab_bar():
                            with dpg.tab(label="MACROS"):
                                for title, items in LAYOUT:
                                    dpg.add_text(title, color=GRAY)
                                    for name, fmt in items:
                                        lo, hi = M.LIMITS[name]
                                        cb = (lambda s, a, u: self.set_macro(u, a))
                                        if name == "DRIVE_TYPE":
                                            dpg.add_combo([n for n, _ in M.DRIVE_TYPES], label="DRIVE TYPE", width=200,
                                                          default_value=M.DRIVE_TYPES[0][0], tag="drive_combo",
                                                          callback=lambda s, a: self.set_macro(
                                                              "DRIVE_TYPE", [n for n, _ in M.DRIVE_TYPES].index(a)))
                                        elif name == "QUARTER":
                                            dpg.add_checkbox(label="QUARTER TONES (24-EDO cluster)", tag="s_QUARTER",
                                                             default_value=bool(self.ui_state[name]),
                                                             callback=lambda s, a: self.set_macro("QUARTER", int(a)))
                                        elif name in M.INT_MACROS:
                                            dpg.add_slider_int(label=name.replace("_", " "), tag=f"s_{name}", width=200,
                                                               min_value=int(lo), max_value=int(hi),
                                                               default_value=int(self.ui_state[name]), user_data=name,
                                                               callback=cb, format=fmt)
                                        else:
                                            dpg.add_slider_float(label=name.replace("_", " "), tag=f"s_{name}", width=200,
                                                                 min_value=lo, max_value=hi, default_value=self.ui_state[name],
                                                                 user_data=name, callback=cb, format=fmt)
                                    if title == "LOOP / MOD":
                                        dpg.add_combo(SHAPES, label="LFO SHAPE", tag="lfo_shape", width=200,
                                                      default_value=self.lfo_shape, callback=lambda s, a: self._lfo("shape", a))
                                        dpg.add_combo(TARGETS, label="LFO TARGET", tag="lfo_target", width=200,
                                                      default_value=self.lfo_target, callback=lambda s, a: self._lfo("target", a))
                                        with dpg.group(horizontal=True):
                                            dpg.add_button(label="LOOP CLEAR", callback=lambda: self.eng.q.put(("loop_clear", 0, 0)))
                                            dpg.add_checkbox(label="FREEZE", tag="freeze_cb", callback=lambda s, a: self.eng.q.put(("loop_freeze", bool(a), None)))
                                    dpg.add_spacer(height=4)
                            with dpg.tab(label="VOICE A"):
                                self.build_raw("A")
                            with dpg.tab(label="VOICE B"):
                                self.build_raw("B")
                            with dpg.tab(label="ROUTING", tag="tab_routing"):
                                self.build_routing()
                    # ---- right: 2x2 analysis
                    with dpg.child_window(width=-1, height=-60, border=False):
                        with dpg.table(header_row=False, borders_innerV=False):
                            dpg.add_table_column(); dpg.add_table_column()
                            with dpg.table_row():
                                y = self.plot("scope", "SCOPE", "ms", "amp", ylim=(-1, 1))
                                dpg.add_line_series([0, 1], [0, 0], parent=y, tag="scope_line")
                                y = self.plot("spec", "SPECTRUM", "Hz", "dB", xlog=True, ylim=(-100, 0), xlim=(20, 20000))
                                dpg.add_line_series([20, 20000], [-100, -100], parent=y, tag="spec_peak")
                                dpg.bind_item_theme("spec_peak", self.gray_line)
                                dpg.add_line_series([20, 20000], [-100, -100], parent=y, tag="spec_line")
                            with dpg.table_row():
                                y = self.plot("sgram", "SPECTROGRAM  (Fig 1)", "", "", ylim=(0, 1), xlim=(0, 1))
                                dpg.add_image_series("sg_tex", [0, 0], [1, 1], parent=y)
                                dpg.set_axis_ticks("sgram_y", tuple((f"{k}k", k / 8) for k in range(0, 9, 2)))
                                dpg.set_axis_ticks("sgram_x", tuple((f"{-5 + i}s", i / 5) for i in range(6)))
                                y = self.plot("rough", "ROUGHNESS · shaded = scream band 30-150 Hz (Fig 2)", "AM rate Hz", "",
                                              xlog=True, ylim=(0, 1.05), xlim=(1, 300))
                                dpg.add_shade_series([30, 150], [1.05, 1.05], y2=[0, 0], parent=y, tag="band")
                                dpg.bind_item_theme("band", self.band_fill)
                                dpg.add_line_series([1, 300], [0, 0], parent=y, tag="rough_line")
                        dpg.add_text("", tag="readout")
                dpg.add_separator()
                with dpg.group(horizontal=True):
                    dpg.add_combo(self.preset_names(), tag="preset_list", width=180, default_value="")
                    dpg.add_button(label="LOAD", callback=lambda: self.load_preset(dpg.get_value("preset_list"))
                                   if dpg.get_value("preset_list") else None)
                    dpg.add_button(label="KEEP", callback=self.keep_preset)
                    dpg.add_text(" | ")
                    dpg.add_combo(list(GROUPS) + ["ALL"], tag="rand_group", width=100, default_value="SCREAM")
                    dpg.add_combo(list(INTENSITY), tag="rand_int", width=70, default_value="MID")
                    dpg.add_button(label="RANDOMIZE", callback=lambda: self.randomize("roll"))
                    dpg.add_button(label="REPEAT", callback=lambda: self.randomize("repeat"))
                    dpg.add_button(label="UNDO", callback=lambda: self.randomize("undo"))
                    dpg.add_text(" | ")
                    dpg.add_checkbox(label="HOLD", tag="hold", callback=lambda s, a: self._hold(a))
                    dpg.add_button(label="PANIC", callback=self.eng.panic)
                    dpg.add_text("", tag="kbd")
        with dpg.handler_registry():
            for i, k in enumerate(KEYMAP):
                key = getattr(dpg, f"mvKey_{k}", None)
                if key is None:
                    continue
                dpg.add_key_press_handler(key, callback=self._kd, user_data=i)
                dpg.add_key_release_handler(key, callback=self._ku, user_data=i)
            dpg.add_key_press_handler(dpg.mvKey_Up, callback=lambda: self._oct(1))
            dpg.add_key_press_handler(dpg.mvKey_Down, callback=lambda: self._oct(-1))
            dpg.add_key_press_handler(dpg.mvKey_Spacebar, callback=self.eng.panic)
        dpg.create_viewport(title="CHAOS — a complex synth voice", width=W, height=H, clear_color=WHITE)
        dpg.setup_dearpygui()
        dpg.show_viewport()
        dpg.set_primary_window("main", True)

    def macro_slider(self, name, fmt="%.2f", label=None):
        lo, hi = M.LIMITS[name]
        cb = (lambda s, a, u: self.set_macro(u, a))
        lab = label or name.replace("_", " ")
        if name in M.INT_MACROS:
            if hi - lo == 1:
                dpg.add_checkbox(label=lab, tag=f"s_{name}", default_value=bool(self.ui_state[name]),
                                 callback=lambda s, a, u: self.set_macro(u, int(a)), user_data=name)
            else:
                dpg.add_slider_int(label=lab, tag=f"s_{name}", width=200, min_value=int(lo), max_value=int(hi),
                                   default_value=int(self.ui_state[name]), user_data=name, callback=cb)
        else:
            dpg.add_slider_float(label=lab, tag=f"s_{name}", width=200, min_value=lo, max_value=hi,
                                 default_value=self.ui_state[name], user_data=name, callback=cb, format=fmt)

    def build_raw(self, voice):
        if voice == "B":
            dpg.add_combo(list(V.B_TYPES), label="B TYPE", width=200, default_value=self.eng.b_type,
                          callback=lambda s, a: (self.eng.q.put(("btype", a, None)), setattr(self, "_resync", 12)))
        dpg.add_text("0..1 sliders mapped to each parameter's live range", color=GRAY)
        for title, keys in V.RAW_GROUPS:
            with dpg.collapsing_header(label=title, default_open=title in ("OSC 1", "AMP ENV")):
                for k in keys:
                    if k not in self.eng.raw[voice]:
                        continue
                    dpg.add_slider_float(label=k, tag=f"raw_{voice}_{k}", width=170, min_value=0, max_value=1,
                                         default_value=self.eng.raw_norm(voice, k), format="%.3f",
                                         callback=lambda s, a, u: self.eng.q.put(("raw", u[0], (u[1], a))),
                                         user_data=(voice, k))

    def build_routing(self):
        dpg.add_text("GENERATOR BUS / A + B", color=GRAY)
        for voice in ("A", "B"):
            dpg.add_text(voice + " / SURGE XT", color=GRAY)
            for key, f in (("MUTE", "%d"), ("LEVEL", "%.1f dB"), ("PAN", "%+.2f")):
                self.macro_slider(f"{voice}_{key}", f)
        dpg.add_separator()
        dpg.add_text("EXPERIMENTAL BUS TOPOLOGY — not finished algorithms", color=GRAY)
        for name in ("ROUTE_MODE", "ROUTE_DEPTH", "BODY_MIX", "BODY_TONE", "BODY_FEED", "EXT_MIX"):
            self.macro_slider(name)
        dpg.add_text("ROUTE MODE: 0=PARALLEL  1=RING/AM  2=DELAY-BEND", color=GRAY)
        dpg.add_separator()
        dpg.add_text("CONTROL-RATE MODULATION MATRIX", color=GRAY)
        dpg.add_text("Four slots / bipolar amounts / changes applied to macro state", color=GRAY)
        for i in range(1, 5):
            dpg.add_text("SLOT " + str(i), color=GRAY)
            for typ, names in (("SRC", MATRIX_SOURCES), ("DST", MATRIX_TARGETS)):
                key = f"MOD{i}_{typ}"
                dpg.add_combo(list(names), label=key, width=180, tag=f"route_{key}",
                              default_value=names[int(self.ui_state[key])],
                              callback=lambda s, a, u: self.set_macro(u[0], u[1].index(a)),
                              user_data=(key, names))
            self.macro_slider(f"MOD{i}_AMT", "%+.2f")
            dpg.add_spacer(height=4)

    def sync_raw(self):
        for voice in ("A", "B"):
            for k in self.eng.raw[voice]:
                tag = f"raw_{voice}_{k}"
                if dpg.does_item_exist(tag):
                    dpg.configure_item(tag, label=self.eng.labels[voice].get(k, k))
                    if getattr(self, "_resync", 0) == 1:
                        dpg.set_value(tag, self.eng.raw_norm(voice, k))

    def _lfo(self, what, v):
        if what == "shape":
            self.lfo_shape = v
        else:
            self.lfo_target = v
        self.eng.q.put(("lfo", what, v))

    def _hold(self, on):
        self.hold = on
        if not on:
            self.eng.panic()

    def _oct(self, d):
        self.octave = max(0, min(8, self.octave + d))

    def _kd(self, s, a, i):
        if i in self.down:
            return
        self.down.add(i)
        self.note(i, True)

    def _ku(self, s, a, i):
        self.down.discard(i)
        self.note(i, False)

    # ------------------------------------------------------------------ live views
    def update_views(self):
        x = self.eng.recent(A.SR)
        st = self.eng.eff
        root = self.eng.held[-1] if self.eng.held else self.eng.last_note
        f0 = A.midi_to_hz(root) * 2 ** (st["OFFSET"] / 1200)
        t, w = A.scope(x, f0)
        dpg.set_value("scope_line", [t.tolist(), w.tolist()])
        dpg.set_axis_limits("scope_x", 0, float(t[-1]) if t[-1] > 0 else 1)
        m = max(0.02, float(np.max(np.abs(w))) * 1.15)
        dpg.set_axis_limits("scope_y", -m, m)
        fl, db = A.spectrum(x)
        self.peak_hold = db if self.peak_hold is None else np.maximum(db, self.peak_hold - 0.4)
        dpg.set_value("spec_line", [fl.tolist(), db.tolist()])
        dpg.set_value("spec_peak", [fl.tolist(), self.peak_hold.tolist()])
        col = A.spectrogram_column(x, rows=SG_H)
        self.sg = np.roll(self.sg, -1, axis=1)
        g = 1.0 - col[::-1]  # dark = energy, high Hz at the top
        self.sg[:, -1, 0] = g; self.sg[:, -1, 1] = g; self.sg[:, -1, 2] = g; self.sg[:, -1, 3] = 1
        dpg.set_value("sg_tex", self.sg.ravel())
        rl, e, frac = A.roughness(x)
        dpg.set_value("rough_line", [rl.tolist(), e.tolist()])
        beat = A.beat_rate(f0, st["DETUNE"]) if st["DETUNE"] > 0.05 else 0.0
        rms, pk = A.levels(x[-4096:])
        cpu = self.stream.cpu_load * 100 if self.stream else 0.0
        ratio = 2 ** (7 * st["FM_RATIO"] / 12)
        dpg.set_value("readout",
                      f"NOTE   {root:3d}  {f0:8.2f} Hz   oct {self.octave}\n"
                      f"BEAT   {beat:6.2f} Hz  {A.beat_zone(beat) if beat else '-'}  (Fig 4)\n"
                      f"SCREAM {frac*100:5.1f} %  of AM energy in 30-150 Hz\n"
                      f"FM     ratio 1:{ratio:.3f}  {'harmonic' if st['FM_RATIO'] < 0.01 else 'inharmonic'}\n"
                      f"PITCH  {self.eng.fall_cents + st['OFFSET']:+7.1f} c (offset+fall)\n"
                      f"LEVEL  rms {rms:6.1f}  peak {pk:6.1f} dBFS\n"
                      f"CPU    {cpu:4.1f} %   voices {len(self.eng.sounding)}\n"
                      f"LFO    {self.eng.lfo.out:+.2f} -> {self.lfo_target}")
        dpg.set_value("kbd", f"  keys Z..M  oct {self.octave}  held {self.eng.held}")

    def run(self, frames=None, screenshot=None):
        self.build()
        self.start_audio()
        n = 0
        demo = getattr(self, "_demo", None)
        if demo == "ROUTING":
            dpg.set_value(dpg.get_item_parent(dpg.get_alias_id("tab_routing")), "tab_routing")
        elif demo:
            self.load_preset(demo)
        while dpg.is_dearpygui_running():
            if n % 2 == 0:
                self.update_views()
            if n % 15 == 0:
                self.sync_raw()
            if getattr(self, "_resync", 0) > 0:
                self._resync -= 1
            dpg.render_dearpygui_frame()
            n += 1
            if frames and n == frames:
                if screenshot:
                    dpg.output_frame_buffer(file=screenshot)
                    for _ in range(3):
                        dpg.render_dearpygui_frame()
                break
        if self.stream:
            self.stream.stop()
        dpg.destroy_context()


if __name__ == "__main__":
    app = App(audio="--no-audio" not in sys.argv)
    frames = shot = None
    if "--screenshot" in sys.argv:
        shot = sys.argv[sys.argv.index("--screenshot") + 1]
        frames = 400
        app._demo = sys.argv[sys.argv.index("--screenshot") + 2] if len(sys.argv) > sys.argv.index("--screenshot") + 2 else "2_shower_strings"
    app.run(frames, shot)
