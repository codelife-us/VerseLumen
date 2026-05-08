#!/usr/bin/env python3
"""textimagelive — two-window live preview for textimage"""

import tkinter as tk
import tkinter.font as tkfont
from tkinter import ttk, colorchooser, filedialog, simpledialog, messagebox
import subprocess
import threading
import os
import sys
import re
import shlex
import shutil
import tempfile
from pathlib import Path

try:
    from PIL import Image, ImageTk, ImageFont
except ImportError:
    print("Pillow is required: pip install pillow")
    sys.exit(1)

TEXTIMAGE   = str(Path(__file__).parent / ("textimage.exe" if sys.platform == "win32" else "textimage"))
TMP_JPG     = tempfile.mktemp(suffix=".jpg", prefix="textimagelive_")
CONFIG_FILE = ".verselumen"

PREVIEW_W = 960
PREVIEW_H = 540


def load_textimage_config(path: str = CONFIG_FILE) -> dict:
    """Parse the [textimage] section of .verselumen."""
    cfg: dict = {}
    try:
        with open(path, encoding="utf-8-sig") as fh:
            in_section = False
            for line in fh:
                line = line.rstrip("\r\n").lstrip(" \t")
                if line.startswith("["):
                    end = line.find("]")
                    sec = line[1:end] if end != -1 else ""
                    in_section = (sec == "textimage")
                    continue
                if not in_section or not line or line.startswith("#"):
                    continue
                eq = line.find("=")
                if eq == -1:
                    continue
                key = line[:eq].rstrip(" \t")
                val = line[eq + 1:].lstrip(" \t")
                if key:
                    cfg[key] = val
    except FileNotFoundError:
        pass
    return cfg


# (file_key, var_attr, "str"|"bool", fallback_default)
THEME_KEYS = [
    ("font",             "font_var",              "str",  ""),
    ("bg",               "bg_var",                "str",  "black"),
    ("textcolor",        "textcolor_var",         "str",  "white"),
    ("bgphoto",          "bgphoto_var",           "str",  ""),
    ("dim",              "dim_var",               "str",  "50"),
    ("width",            "width_var",             "str",  "1920"),
    ("height",           "height_var",            "str",  "1080"),
    ("textoffy",         "textoffy_var",          "str",  "0"),
    ("maxtextsize",      "maxtextsize_var",       "str",  ""),
    ("textsize",         "textsize_var",          "str",  ""),
    ("textscale",        "textscale_var",         "str",  ""),
    ("textpanel",        "textpanel_var",         "str",  ""),
    ("textpanelcolor",   "textpanelcolor_var",    "str",  "black"),
    ("textshadow",       "textshadow_var",        "str",  "0"),
    ("shadowmethod",     "shadowmethod_var",      "str",  "1"),
    ("textoutline",      "textoutline_var",       "str",  "0"),
    ("textoutlinecolor", "textoutlinecolor_var",  "str",  "black"),
    ("linespacing",      "linespacing_var",       "str",  "0"),
    ("textpanelrounded", "panelrounded_var",      "bool", "no"),
    ("reservetop",       "reserve_top_var",       "str",  "0"),
    ("reserveright",     "reserve_right_var",     "str",  "0"),
    ("reservebottom",    "reserve_bottom_var",    "str",  "0"),
    ("reserveleft",      "reserve_left_var",      "str",  "0"),
    ("text2color",        "text2color_var",         "str",  ""),
    ("text2font",         "text2font_var",          "str",  ""),
    ("text2gap",          "text2gap_var",           "str",  "40"),
    ("text2outline",      "text2outline_var",       "str",  ""),
    ("text2outlinecolor", "text2outlinecolor_var",  "str",  ""),
    ("text2shadow",       "text2shadow_var",        "str",  ""),
    ("text2shadowmethod", "text2shadowmethod_var",  "str",  ""),
]


def find_live_path() -> str:
    local = CONFIG_FILE
    if os.path.exists(local):
        return local
    home = os.path.join(os.path.expanduser("~"), CONFIG_FILE)
    if os.path.exists(home):
        return home
    return local


def load_live_state(path: str) -> dict:
    """Parse [textimagelive] section of .verselumen."""
    state: dict = {"default_theme": "", "themes": {}}
    try:
        with open(path, encoding="utf-8-sig") as fh:
            in_section = False
            current: str | None = None
            for line in fh:
                line = line.rstrip("\r\n").lstrip(" \t")
                if line.startswith("["):
                    end = line.find("]")
                    sec = line[1:end] if end != -1 else ""
                    if ":" not in sec:
                        in_section = (sec == "textimagelive")
                        current = None
                        continue
                    if in_section:
                        m = re.match(r'^\[theme:(.+)\]$', line)
                        if m:
                            current = m.group(1)
                            state["themes"].setdefault(current, {})
                    continue
                if not in_section or not line or line.startswith("#"):
                    continue
                eq = line.find("=")
                if eq == -1:
                    continue
                key = line[:eq].rstrip(" \t")
                val = line[eq + 1:].lstrip(" \t")
                if current is None:
                    if key in ("default_theme", "half_size", "font_favorites", "ui_scale"):
                        state[key] = val
                else:
                    state["themes"][current][key] = val
    except FileNotFoundError:
        pass
    return state


def save_live_state(path: str, state: dict):
    """Write [textimagelive] section of .verselumen, preserving all other sections."""
    new_lines: list[str] = []
    if state.get("default_theme"):
        new_lines.append(f"default_theme = {state['default_theme']}\n")
    if "half_size" in state:
        new_lines.append(f"half_size = {state['half_size']}\n")
    if state.get("ui_scale") and state["ui_scale"] != "1.0":
        new_lines.append(f"ui_scale = {state['ui_scale']}\n")
    if state.get("font_favorites"):
        new_lines.append(f"font_favorites = {state['font_favorites']}\n")
    for name in sorted(state["themes"]):
        new_lines.append(f"\n[theme:{name}]\n")
        for k, v in state["themes"][name].items():
            new_lines.append(f"{k} = {v}\n")

    before: list[str] = []
    after: list[str] = []
    in_target = False
    found = False
    try:
        with open(path, encoding="utf-8-sig") as fh:
            for line in fh:
                stripped = line.rstrip("\r\n").lstrip(" \t")
                if stripped.startswith("["):
                    end = stripped.find("]")
                    sec = stripped[1:end] if end != -1 else ""
                    if ":" not in sec:
                        if sec == "textimagelive":
                            in_target = True
                            found = True
                            continue
                        else:
                            in_target = False
                if in_target:
                    continue
                (after if found else before).append(line)
    except FileNotFoundError:
        pass

    while before and not before[-1].strip():
        before.pop()
    while after and not after[0].strip():
        after.pop(0)

    with open(path, "w", encoding="utf-8") as fh:
        fh.writelines(before)
        if before:
            fh.write("\n")
        fh.write("[textimagelive]\n")
        fh.writelines(new_lines)
        if after:
            fh.write("\n")
            fh.writelines(after)


class TextImageView:
    def __init__(self):
        # ── Control window ────────────────────────────────────────────────────
        self.root = tk.Tk()
        self.root.title("textimagelive — Controls")
        self.root.resizable(False, False)
        self.root.protocol("WM_DELETE_WINDOW", self._quit)

        # ── Preview window ────────────────────────────────────────────────────
        self.win = tk.Toplevel(self.root)
        self.win.title("textimagelive — Preview")
        self.win.resizable(True, True)
        self.win.protocol("WM_DELETE_WINDOW", self._quit)

        # ── State ─────────────────────────────────────────────────────────────
        self._after_id    = None
        self._render_gen  = 0
        self._proc        = None
        self._photo       = None
        self._live_active = True

        cfg = load_textimage_config()

        local_cfg = CONFIG_FILE
        home_cfg  = os.path.join(os.path.expanduser("~"), CONFIG_FILE)
        both_exist = (os.path.exists(local_cfg) and
                      os.path.exists(home_cfg) and
                      os.path.abspath(local_cfg) != os.path.abspath(home_cfg))
        if both_exist:
            use_local = messagebox.askyesno(
                "Multiple config files found",
                f"A local {CONFIG_FILE} exists in this folder as well as {home_cfg}.\n\n"
                f"Load the local version?",
                parent=self.root)
            self._live_path = local_cfg if use_local else home_cfg
        else:
            self._live_path = find_live_path()
        self._live_state = load_live_state(self._live_path)
        self.ui_scale = float(self._live_state.get("ui_scale", "1.0"))
        self._log_path = os.path.join(
            os.path.dirname(os.path.abspath(self._live_path)), ".verselumen.log"
        )

        def _cfg(key, default=""):
            return cfg.get(key, default)

        maxtextsize  = int(_cfg("maxtextsize", "0"))
        textsize_val = int(_cfg("textsize",    "0"))
        textscale    = int(_cfg("textscale",   "100"))

        self.maxtextsize_var    = tk.StringVar(value=str(maxtextsize)  if maxtextsize  != 0 else "")
        self.textsize_var       = tk.StringVar(value=str(textsize_val) if textsize_val != 0 else "")
        self.textscale_var      = tk.StringVar(value=str(textscale)    if textscale   != 100 else "")
        self.font_var           = tk.StringVar(value=_cfg("font",      ""))
        self.width_var          = tk.StringVar(value=_cfg("width",     "1920"))
        self.height_var         = tk.StringVar(value=_cfg("height",    "1080"))
        self.textoffy_var       = tk.StringVar(value=_cfg("textoffy",  "0"))
        self.bg_var             = tk.StringVar(value=_cfg("bg",        "black"))
        self.textcolor_var      = tk.StringVar(value=_cfg("textcolor", "white"))
        self.bgphoto_var        = tk.StringVar(value=_cfg("bgphoto",   ""))
        self.dim_var            = tk.StringVar(value=_cfg("dim",       "50"))
        _ts = _cfg("textshadow", "0")
        self.textshadow_var     = tk.StringVar(value="5" if _ts == "yes" else "0" if _ts == "no" else _ts)
        self.shadowmethod_var   = tk.StringVar(value=_cfg("shadowmethod",     "1"))
        self.textoutline_var    = tk.StringVar(value=_cfg("textoutline",      "0"))
        self.textoutlinecolor_var = tk.StringVar(value=_cfg("textoutlinecolor", "black"))
        self.textpanel_var      = tk.StringVar(value=_cfg("textpanel",        ""))
        self.textpanelcolor_var = tk.StringVar(value=_cfg("textpanelcolor",   "black"))
        self.panelrounded_var   = tk.BooleanVar(value=_cfg("textpanelrounded", "no") == "yes")
        self.linespacing_var    = tk.StringVar(value=_cfg("linespacing", "0"))
        self.reserve_top_var    = tk.StringVar(value=_cfg("reservetop",    "0"))
        self.reserve_right_var  = tk.StringVar(value=_cfg("reserveright",  "0"))
        self.reserve_bottom_var = tk.StringVar(value=_cfg("reservebottom", "0"))
        self.reserve_left_var   = tk.StringVar(value=_cfg("reserveleft",   "0"))
        self.text2color_var     = tk.StringVar(value=_cfg("text2color", ""))
        self.text2font_var      = tk.StringVar(value=_cfg("text2font",  ""))
        self.text2gap_var           = tk.StringVar(value=_cfg("text2gap",          "40"))
        self.text2outline_var       = tk.StringVar(value=_cfg("text2outline",      ""))
        self.text2outlinecolor_var  = tk.StringVar(value=_cfg("text2outlinecolor", ""))
        self.text2shadow_var        = tk.StringVar(value=_cfg("text2shadow",       ""))
        self.text2shadowmethod_var  = tk.StringVar(value=_cfg("text2shadowmethod", ""))
        half_size_saved         = self._live_state.get("half_size", "yes")
        self.half_size_var      = tk.BooleanVar(value=half_size_saved != "no")
        self.status_var         = tk.StringVar(value="Ready")

        self._build_controls()
        self._build_preview()
        self._position_windows()

        # Restore last text then apply default theme (which may re-render)
        default_name = self._live_state.get("default_theme", "")
        if default_name and default_name in self._live_state["themes"]:
            self._apply_theme(default_name)
            self.theme_var.set(self._theme_display(default_name))
        else:
            self._schedule(0)

    # ── UI construction ───────────────────────────────────────────────────────

    def _build_controls(self):
        f = self.root
        s = self.ui_scale
        for _fn in ("TkDefaultFont", "TkTextFont"):
            try: tkfont.nametofont(_fn).configure(size=max(9, round(10 * s)))
            except: pass
        pad = dict(padx=round(8 * s), pady=round(3 * s))
        f.columnconfigure(1, minsize=round(110 * s))
        f.columnconfigure(2, minsize=round(110 * s))

        # ── Row 0: Theme + Zoom ──────────────────────────────────────────────────
        self.theme_var = tk.StringVar()
        tk.Label(f, text="Theme:").grid(row=0, column=0, sticky="e", **pad)
        self.theme_cb = ttk.Combobox(f, textvariable=self.theme_var, state="readonly", width=18)
        self.theme_cb.grid(row=0, column=1, sticky="ew", **pad)
        self.theme_cb.bind("<<ComboboxSelected>>", lambda _: self._on_theme_select())
        tbf = tk.Frame(f)
        tbf.grid(row=0, column=2, sticky="w", padx=(4, 0), pady=round(3 * s))
        tk.Button(tbf, text="Save…",   padx=3, command=self._save_theme_dialog).pack(side="left", padx=(0, 3))
        tk.Button(tbf, text="Delete",  padx=3, command=self._delete_theme).pack(side="left", padx=(0, 3))
        tk.Button(tbf, text="Default", padx=3, command=self._make_default_theme).pack(side="left")
        _zoom_f = tk.Frame(f)
        _zoom_f.grid(row=0, column=3, sticky="e", padx=(0, round(6 * s)), pady=round(3 * s))
        tk.Button(_zoom_f, text="−", width=2, command=lambda: self._zoom(-0.1)).pack(side="left")
        tk.Label(_zoom_f, text=f"{round(s * 100)}%", width=4).pack(side="left", padx=2)
        tk.Button(_zoom_f, text="+", width=2, command=lambda: self._zoom(0.1)).pack(side="left")
        self.root.bind("<Control-equal>", lambda _e: self._zoom(0.1))
        self.root.bind("<Control-minus>",  lambda _e: self._zoom(-0.1))
        self.root.bind("<Control-0>",      lambda _e: self._zoom(0))
        self._update_theme_dropdown()

        # ── Row 1: Preview toggle + action buttons ────────────────────────────
        tk.Checkbutton(f, text="Preview at half size", variable=self.half_size_var,
                       command=self._on_half_size_toggle).grid(row=1, column=0, columnspan=2, sticky="w", **pad)
        _act_frame = tk.Frame(f)
        _act_frame.grid(row=1, column=2, columnspan=2, sticky="e", padx=(0, 8), pady=round(3 * s))
        self.live_btn = tk.Button(_act_frame, text="Pause", width=6, command=self._toggle_live)
        self.live_btn.pack(side="left", padx=(0, 4))
        tk.Button(_act_frame, text="Copy cmd", command=self._copy_cmd).pack(side="left", padx=(0, 4))
        tk.Button(_act_frame, text="Save Image…", command=self._save_image).pack(side="left")

        # ── Row 2: Width / Height ─────────────────────────────────────────────
        tk.Label(f, text="Width:").grid(row=2, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.width_var, width=6).grid(row=2, column=1, sticky="w", **pad)
        self.width_var.trace_add("write", lambda *_: self._schedule(600))
        tk.Label(f, text="Height:").grid(row=2, column=2, sticky="e", **pad)
        tk.Entry(f, textvariable=self.height_var, width=6).grid(row=2, column=3, sticky="w", **pad)
        self.height_var.trace_add("write", lambda *_: self._schedule(600))

        # ── Row 3: Max text pt / Text scale % ────────────────────────────────
        tk.Label(f, text="Max text pt:").grid(row=3, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.maxtextsize_var, width=6).grid(row=3, column=1, sticky="w", **pad)
        self.maxtextsize_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Label(f, text="Text scale %:").grid(row=3, column=2, sticky="e", **pad)
        tk.Entry(f, textvariable=self.textscale_var, width=6).grid(row=3, column=3, sticky="w", **pad)
        self.textscale_var.trace_add("write", lambda *_: self._schedule(400))

        # ── Row 4: Text pt / Text off Y ──────────────────────────────────────
        tk.Label(f, text="Text pt:").grid(row=4, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.textsize_var, width=6).grid(row=4, column=1, sticky="w", **pad)
        self.textsize_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Label(f, text="Text off Y:").grid(row=4, column=2, sticky="e", **pad)
        tk.Entry(f, textvariable=self.textoffy_var, width=5).grid(row=4, column=3, sticky="w", **pad)
        self.textoffy_var.trace_add("write", lambda *_: self._schedule(400))

        # ── Row 5: BG color / Text panel color ───────────────────────────────
        self._make_color_row(f, "BG:",               self.bg_var,             5)
        self._make_color_row(f, "Text panel color:", self.textpanelcolor_var, 6)

        # ── Row 7: BG Photo ───────────────────────────────────────────────────
        tk.Label(f, text="BG photo:").grid(row=7, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.bgphoto_var, width=28).grid(row=7, column=1, columnspan=2, sticky="ew", **pad)
        self.bgphoto_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Button(f, text="…", padx=2,
                  command=self._browse_bgphoto).grid(row=7, column=3, sticky="w", padx=(0, 6), pady=round(3 * s))

        # ── Row 8: Dim % / Text panel % + Rounded / Line spacing ─────────────
        tk.Label(f, text="Dim %:").grid(row=8, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.dim_var, width=5).grid(row=8, column=1, sticky="w", **pad)
        self.dim_var.trace_add("write", lambda *_: self._schedule(400))
        tp_frame = tk.Frame(f)
        tp_frame.grid(row=8, column=2, sticky="w", **pad)
        tk.Label(tp_frame, text="Panel %:").pack(side="left")
        tk.Entry(tp_frame, textvariable=self.textpanel_var, width=4).pack(side="left", padx=(2, 4))
        tk.Checkbutton(tp_frame, text="Rounded", variable=self.panelrounded_var,
                       command=lambda: self._schedule(0)).pack(side="left")
        self.textpanel_var.trace_add("write", lambda *_: self._schedule(400))

        # ── Row 9: Line spacing / Reserve % ──────────────────────────────────
        tk.Label(f, text="Line spacing:").grid(row=9, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.linespacing_var, width=5).grid(row=9, column=1, sticky="w", **pad)
        self.linespacing_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Label(f, text="Reserve %:").grid(row=9, column=2, sticky="e", **pad)
        res_frame = tk.Frame(f)
        res_frame.grid(row=9, column=3, sticky="w", **pad)
        for _lbl, _var in (("T", self.reserve_top_var), ("R", self.reserve_right_var),
                            ("B", self.reserve_bottom_var), ("L", self.reserve_left_var)):
            tk.Label(res_frame, text=f"{_lbl}:").pack(side="left")
            tk.Entry(res_frame, textvariable=_var, width=3).pack(side="left", padx=(0, 4))
            _var.trace_add("write", lambda *_: self._schedule(400))

        # ── Row 10: Text 1 ────────────────────────────────────────────────────
        tk.Label(f, text="Text 1:").grid(row=10, column=0, sticky="ne", padx=8, pady=6)
        self.text_widget = tk.Text(f, height=4, width=36, wrap="word",
                                   font=("", round(12 * s)), relief="solid", borderwidth=1)
        self.text_widget.grid(row=10, column=1, columnspan=3, sticky="ew", padx=8, pady=6)
        self.text_widget.bind("<KeyRelease>", lambda _: self._schedule(400))
        self.text_widget.bind("<<Paste>>",    lambda _: self._schedule(500))

        # ── Row 11: Text 1 font / Text gap ───────────────────────────────────
        tk.Label(f, text="Text 1 font:").grid(row=11, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.font_var, width=20).grid(row=11, column=1, sticky="ew", **pad)
        self.font_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Button(f, text="…", padx=2,
                  command=self._browse_font).grid(row=11, column=2, sticky="w", padx=(0, 4), pady=round(3 * s))
        _gap_f = tk.Frame(f)
        _gap_f.grid(row=11, column=3, sticky="w", **pad)
        tk.Label(_gap_f, text="Gap between texts:").pack(side="left", padx=(0, 3))
        tk.Entry(_gap_f, textvariable=self.text2gap_var, width=4).pack(side="left")
        self.text2gap_var.trace_add("write", lambda *_: self._schedule(400))

        # ── Row 12: Text 1 color ──────────────────────────────────────────────
        tk.Label(f, text="Text 1 color:").grid(row=12, column=0, sticky="e", **pad)
        t1c_frame = tk.Frame(f)
        t1c_frame.grid(row=12, column=1, sticky="w", **pad)
        tk.Entry(t1c_frame, textvariable=self.textcolor_var, width=14).pack(side="left")
        t1c_swatch = tk.Label(t1c_frame, width=2, relief="solid", cursor="hand2")
        t1c_swatch.pack(side="left", padx=(4, 2))
        tk.Button(t1c_frame, text="…", padx=2,
                  command=lambda: self._pick_color(self.textcolor_var, t1c_swatch)).pack(side="left")
        def _refresh_t1c(*_):
            color = self.textcolor_var.get().strip() or "gray50"
            try:    t1c_swatch.config(bg=color)
            except: t1c_swatch.config(bg="gray50")
            self._schedule(400)
        self.textcolor_var.trace_add("write", _refresh_t1c)
        t1c_swatch.bind("<Button-1>", lambda _: self._pick_color(self.textcolor_var, t1c_swatch))
        _refresh_t1c()

        # ── Row 13: Text 1 outline px / outline color ─────────────────────────
        tk.Label(f, text="Text 1 outline px:").grid(row=13, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.textoutline_var, width=3).grid(row=13, column=1, sticky="w", **pad)
        self.textoutline_var.trace_add("write", lambda *_: self._schedule(400))
        self._make_color_row(f, "Outline color:", self.textoutlinecolor_var, 13, col_start=2)

        # ── Row 14: Text 1 shadow / shadow method ─────────────────────────────
        tk.Label(f, text="Text 1 shadow (0-10):").grid(row=14, column=0, sticky="e", **pad)
        _shd_frame = tk.Frame(f)
        _shd_frame.grid(row=14, column=1, sticky="w", **pad)
        tk.Entry(_shd_frame, textvariable=self.textshadow_var, width=3).pack(side="left", padx=(0, 6))
        self.textshadow_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Label(f, text="Method:").grid(row=14, column=2, sticky="e", **pad)
        _SM_OPTS = ["1 – Soft (Gaussian)", "2 – Hard (copy)"]
        self._sm_cb = ttk.Combobox(f, values=_SM_OPTS, state="readonly", width=17)
        try:
            self._sm_cb.current(int(self.shadowmethod_var.get()) - 1)
        except (ValueError, IndexError):
            self._sm_cb.current(0)
        self._sm_cb.grid(row=14, column=3, sticky="w", **pad)
        def _on_sm_select(_e=None):
            self.shadowmethod_var.set(str(self._sm_cb.current() + 1))
            self._schedule(0)
        self._sm_cb.bind("<<ComboboxSelected>>", _on_sm_select)
        def _sync_sm_cb(*_):
            try:
                self._sm_cb.current(int(self.shadowmethod_var.get()) - 1)
            except (ValueError, IndexError):
                pass
        self.shadowmethod_var.trace_add("write", _sync_sm_cb)

        # ── Row 15: Text 2 ────────────────────────────────────────────────────
        tk.Label(f, text="Text 2:").grid(row=15, column=0, sticky="ne", padx=8, pady=6)
        self.text2_widget = tk.Text(f, height=3, width=36, wrap="word",
                                    font=("", round(12 * s)), relief="solid", borderwidth=1)
        self.text2_widget.grid(row=15, column=1, columnspan=3, sticky="ew", padx=8, pady=6)
        self.text2_widget.bind("<KeyRelease>", lambda _: self._schedule(400))
        self.text2_widget.bind("<<Paste>>",    lambda _: self._schedule(500))

        # ── Row 16: Text 2 font ───────────────────────────────────────────────
        tk.Label(f, text="Text 2 font:").grid(row=16, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.text2font_var, width=28).grid(row=16, column=1, columnspan=2, sticky="ew", **pad)
        self.text2font_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Button(f, text="…", padx=2,
                  command=self._browse_text2font).grid(row=16, column=3, sticky="w", padx=(0, 6), pady=round(3 * s))

        # ── Row 17: Text 2 color / Gap px ────────────────────────────────────
        tk.Label(f, text="Text 2 color:").grid(row=17, column=0, sticky="e", **pad)
        t2c_frame = tk.Frame(f)
        t2c_frame.grid(row=17, column=1, sticky="w", **pad)
        tk.Entry(t2c_frame, textvariable=self.text2color_var, width=14).pack(side="left")
        t2c_swatch = tk.Label(t2c_frame, width=2, relief="solid", cursor="hand2")
        t2c_swatch.pack(side="left", padx=(4, 2))
        tk.Button(t2c_frame, text="…", padx=2,
                  command=lambda: self._pick_color(self.text2color_var, t2c_swatch)).pack(side="left")
        def _refresh_t2c(*_):
            color = self.text2color_var.get().strip() or "gray50"
            try:    t2c_swatch.config(bg=color)
            except: t2c_swatch.config(bg="gray50")
            self._schedule(400)
        self.text2color_var.trace_add("write", _refresh_t2c)
        t2c_swatch.bind("<Button-1>", lambda _: self._pick_color(self.text2color_var, t2c_swatch))
        _refresh_t2c()

        # ── Row 18: Text 2 outline px / outline color ─────────────────────────
        tk.Label(f, text="Text 2 outline px:").grid(row=18, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.text2outline_var, width=3).grid(row=18, column=1, sticky="w", **pad)
        self.text2outline_var.trace_add("write", lambda *_: self._schedule(400))
        self._make_color_row(f, "T2 outline color:", self.text2outlinecolor_var, 18, col_start=2)

        # ── Row 19: Text 2 shadow / shadow method ─────────────────────────────
        tk.Label(f, text="Text 2 shadow (0-10):").grid(row=19, column=0, sticky="e", **pad)
        _t2shd_frame = tk.Frame(f)
        _t2shd_frame.grid(row=19, column=1, sticky="w", **pad)
        tk.Entry(_t2shd_frame, textvariable=self.text2shadow_var, width=3).pack(side="left", padx=(0, 6))
        self.text2shadow_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Label(f, text="T2 method:").grid(row=19, column=2, sticky="e", **pad)
        _T2SM_OPTS = ["1 – Soft (Gaussian)", "2 – Hard (copy)"]
        self._t2sm_cb = ttk.Combobox(f, values=_T2SM_OPTS, state="readonly", width=17)
        try:
            idx = int(self.text2shadowmethod_var.get() or "1") - 1
            self._t2sm_cb.current(max(0, min(1, idx)))
        except (ValueError, IndexError):
            self._t2sm_cb.current(0)
        self._t2sm_cb.grid(row=19, column=3, sticky="w", **pad)
        def _on_t2sm_select(_e=None):
            self.text2shadowmethod_var.set(str(self._t2sm_cb.current() + 1))
            self._schedule(0)
        self._t2sm_cb.bind("<<ComboboxSelected>>", _on_t2sm_select)
        def _sync_t2sm_cb(*_):
            try:
                self._t2sm_cb.current(int(self.text2shadowmethod_var.get() or "1") - 1)
            except (ValueError, IndexError):
                pass
        self.text2shadowmethod_var.trace_add("write", _sync_t2sm_cb)

        # ── Row 20: Status ────────────────────────────────────────────────────
        tk.Label(f, textvariable=self.status_var, fg="gray45",
                 anchor="w", width=46).grid(row=20, column=0, columnspan=4, **pad)

    def _make_color_row(self, parent, label: str, var: tk.StringVar, row: int, col_start: int = 0):
        s = getattr(self, 'ui_scale', 1.0)
        pad = dict(padx=round(6 * s), pady=round(3 * s))
        tk.Label(parent, text=label).grid(row=row, column=col_start, sticky="e", **pad)

        entry = tk.Entry(parent, textvariable=var, width=22 if col_start == 0 else 10)
        entry.grid(row=row, column=col_start + 1, columnspan=(2 if col_start == 0 else 1), sticky="ew", **pad)

        cf = tk.Frame(parent)
        cf.grid(row=row, column=col_start + (3 if col_start == 0 else 2), padx=(0, 6), pady=3, sticky="w")
        swatch = tk.Label(cf, width=2, relief="solid", cursor="hand2")
        swatch.pack(side="left", padx=(0, 2))
        tk.Button(cf, text="…", padx=2,
                  command=lambda: self._pick_color(var, swatch)).pack(side="left")

        def refresh_swatch(*_):
            color = var.get().strip() or "gray50"
            try:
                swatch.config(bg=color)
            except tk.TclError:
                swatch.config(bg="gray50")
            self._schedule(400)

        var.trace_add("write", refresh_swatch)
        swatch.bind("<Button-1>", lambda _: self._pick_color(var, swatch))
        refresh_swatch()

    # ── Theme helpers ─────────────────────────────────────────────────────────

    def _theme_display(self, name: str) -> str:
        default = self._live_state.get("default_theme", "")
        return f"{name} (default)" if name == default else name

    def _theme_from_display(self, display: str) -> str:
        if display.endswith(" (default)"):
            return display[:-len(" (default)")]
        return display

    def _update_theme_dropdown(self):
        names = sorted(self._live_state["themes"])
        values = [self._theme_display(n) for n in names]
        self.theme_cb["values"] = values
        bare = self._theme_from_display(self.theme_var.get())
        if bare in self._live_state["themes"]:
            self.theme_var.set(self._theme_display(bare))
        elif not bare:
            self.theme_var.set("")

    def _apply_theme(self, name: str):
        theme = self._live_state["themes"][name]
        for key, attr, typ, default in THEME_KEYS:
            val = theme.get(key, default)
            var = getattr(self, attr)
            if typ == "bool":
                var.set(val in ("yes", "true", "1"))
            elif key == "textshadow":
                var.set("5" if val == "yes" else "0" if val in ("no", "") else val)
            else:
                var.set(val)
        if "text" in theme:
            self.text_widget.delete("1.0", "end")
            self.text_widget.insert("1.0", theme["text"].replace("\\n", "\n"))
        if "text2" in theme:
            self.text2_widget.delete("1.0", "end")
            self.text2_widget.insert("1.0", theme["text2"].replace("\\n", "\n"))

    def _collect_theme(self) -> dict:
        theme = {}
        for key, attr, typ, _ in THEME_KEYS:
            var = getattr(self, attr)
            theme[key] = ("yes" if var.get() else "no") if typ == "bool" else var.get()
        theme["text"]  = self.text_widget.get("1.0", "end-1c").replace("\n", "\\n")
        theme["text2"] = self.text2_widget.get("1.0", "end-1c").replace("\n", "\\n")
        return theme

    def _on_theme_select(self):
        name = self._theme_from_display(self.theme_var.get())
        if name and name in self._live_state["themes"]:
            self._apply_theme(name)
            self._schedule(0)

    def _save_theme_dialog(self):
        initial = self._theme_from_display(self.theme_var.get()) or "My Theme"
        name = simpledialog.askstring("Save Theme", "Theme name:", initialvalue=initial, parent=self.root)
        if not name or not name.strip():
            return
        name = name.strip()
        self._live_state["themes"][name] = self._collect_theme()
        save_live_state(self._live_path, self._live_state)
        self._update_theme_dropdown()
        self.theme_var.set(self._theme_display(name))

    def _delete_theme(self):
        name = self._theme_from_display(self.theme_var.get())
        if not name or name not in self._live_state["themes"]:
            return
        if not messagebox.askyesno("Delete Theme", f"Delete theme \"{name}\"?", parent=self.root):
            return
        del self._live_state["themes"][name]
        if self._live_state.get("default_theme") == name:
            self._live_state["default_theme"] = ""
        save_live_state(self._live_path, self._live_state)
        self._update_theme_dropdown()
        self.theme_var.set("")

    def _make_default_theme(self):
        name = self._theme_from_display(self.theme_var.get())
        if not name or name not in self._live_state["themes"]:
            return
        self._live_state["default_theme"] = name
        save_live_state(self._live_path, self._live_state)
        self._update_theme_dropdown()

    # ── Font / file pickers ───────────────────────────────────────────────────

    def _browse_font(self):
        if sys.platform == "win32":
            win_fonts = os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts")
            path = self._font_list_picker([win_fonts]) or self._pick_font_file(win_fonts)
        else:
            dirs = [
                Path.home() / "Library" / "Fonts",
                Path("/Library/Fonts"),
                Path("/System/Library/Fonts"),
            ]
            path = self._font_list_picker([str(d) for d in dirs if d.exists()])
        if path:
            self.font_var.set(path)

    def _browse_text2font(self):
        if sys.platform == "win32":
            win_fonts = os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts")
            path = self._font_list_picker([win_fonts], self.text2font_var) or self._pick_font_file(win_fonts)
        else:
            dirs = [
                Path.home() / "Library" / "Fonts",
                Path("/Library/Fonts"),
                Path("/System/Library/Fonts"),
            ]
            path = self._font_list_picker([str(d) for d in dirs if d.exists()], self.text2font_var)
        if path:
            self.text2font_var.set(path)

    def _font_list_picker(self, dirs: list, target_var: tk.StringVar = None) -> str:
        """Searchable font picker with All / Favorites views and live preview."""
        if target_var is None:
            target_var = self.font_var
        exts = {".ttf", ".otf", ".ttc"}
        entries: list = []
        labels = {
            str(Path.home() / "Library" / "Fonts"): "Personal",
            "/Library/Fonts":                         "System",
            "/System/Library/Fonts":                  "Built-in",
        }
        for d in dirs:
            label = labels.get(d, os.path.basename(d))
            try:
                for name in sorted(os.listdir(d), key=str.lower):
                    if os.path.splitext(name)[1].lower() in exts:
                        display = os.path.splitext(name)[0]
                        entries.append((display, os.path.join(d, name), label))
            except OSError:
                pass
        entries.sort(key=lambda x: x[0].lower())
        if not entries:
            return ""

        current_path = target_var.get().strip()
        original_font = current_path
        favs: set = set(filter(None, self._live_state.get("font_favorites", "").split("|")))
        _preview_id = [None]

        dlg = tk.Toplevel(self.root)
        dlg.title("Select Font")
        dlg.grab_set()
        dlg.resizable(True, True)
        result: list = []

        frame = ttk.Frame(dlg, padding=8)
        frame.pack(fill="both", expand=True)
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(2, weight=1)

        # Search bar + Preview toggle (row 0)
        sf = ttk.Frame(frame)
        sf.grid(row=0, column=0, sticky="ew", pady=(0, 4))
        sf.columnconfigure(1, weight=1)
        ttk.Label(sf, text="Search:").grid(row=0, column=0, padx=(0, 6))
        search_var = tk.StringVar()
        search_entry = ttk.Entry(sf, textvariable=search_var)
        search_entry.grid(row=0, column=1, sticky="ew")
        preview_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(sf, text="Preview", variable=preview_var).grid(row=0, column=2, padx=(10, 0))

        # View toggle (row 1)
        view_var = tk.StringVar(value="favorites" if favs else "all")
        vf = ttk.Frame(frame)
        vf.grid(row=1, column=0, sticky="w", pady=(0, 4))
        ttk.Label(vf, text="View:").pack(side="left", padx=(0, 6))
        ttk.Radiobutton(vf, text="All Fonts", variable=view_var, value="all",
                        command=lambda: populate(search_var.get())).pack(side="left")
        ttk.Radiobutton(vf, text="Favorites", variable=view_var, value="favorites",
                        command=lambda: populate(search_var.get())).pack(side="left", padx=(6, 0))

        # Listbox (row 2)
        lbf = ttk.Frame(frame)
        lbf.grid(row=2, column=0, sticky="nsew")
        lbf.columnconfigure(0, weight=1)
        lbf.rowconfigure(0, weight=1)
        sb = ttk.Scrollbar(lbf, orient="vertical")
        lb = tk.Listbox(lbf, yscrollcommand=sb.set, width=56, height=26,
                        selectmode="single", activestyle="dotbox")
        sb.config(command=lb.yview)
        sb.pack(side="right", fill="y")
        lb.pack(side="left", fill="both", expand=True)

        visible: list = []

        def _selected_path() -> str:
            sel = lb.curselection()
            return visible[sel[0]][1] if sel else ""

        def _refresh_fav_btn():
            p = _selected_path()
            fav_btn.config(text="★ Remove Favorite" if p in favs else "☆ Add to Favorites")

        def _on_select(_=None):
            _refresh_fav_btn()
            if not preview_var.get():
                return
            p = _selected_path()
            if p and p != target_var.get():
                target_var.set(p)
                if _preview_id[0]:
                    self.root.after_cancel(_preview_id[0])
                _preview_id[0] = self.root.after(150, self._render)

        def populate(filter_str: str = ""):
            nonlocal visible
            lb.delete(0, "end")
            fl = filter_str.lower()
            pool = [(d, p, s) for d, p, s in entries if p in favs] \
                   if view_var.get() == "favorites" else entries
            visible = [(d, p, s) for d, p, s in pool if not fl or fl in d.lower()]
            for display, path, src in visible:
                prefix = "★ " if path in favs else "   "
                lb.insert("end", f"{prefix}{display}  [{src}]")
            if current_path:
                cur_name = os.path.splitext(os.path.basename(current_path))[0]
                for i, (d, _, _s) in enumerate(visible):
                    if d == cur_name:
                        lb.selection_set(i)
                        lb.see(i)
                        break
            _refresh_fav_btn()

        def _toggle_fav():
            p = _selected_path()
            if not p:
                return
            if p in favs:
                favs.discard(p)
            else:
                favs.add(p)
            self._live_state["font_favorites"] = "|".join(sorted(favs))
            save_live_state(self._live_path, self._live_state)
            populate(search_var.get())
            for i, (_, fp, _) in enumerate(visible):
                if fp == p:
                    lb.selection_set(i)
                    lb.see(i)
                    break
            _refresh_fav_btn()

        lb.bind("<<ListboxSelect>>", _on_select)

        def on_search(*_):
            sel = lb.curselection()
            sel_path = visible[sel[0]][1] if sel else ""
            populate(search_var.get())
            if sel_path:
                for i, (_, p, _) in enumerate(visible):
                    if p == sel_path:
                        lb.selection_set(i)
                        lb.see(i)
                        break

        search_var.trace_add("write", on_search)
        search_entry.focus_set()

        def cancel():
            if _preview_id[0]:
                self.root.after_cancel(_preview_id[0])
            if preview_var.get() and target_var.get() != original_font:
                target_var.set(original_font)
                self.root.after(0, self._render)
            dlg.destroy()

        def confirm(*_):
            sel = lb.curselection()
            if sel:
                result.append(visible[sel[0]][1])
            dlg.destroy()

        lb.bind("<Double-1>", confirm)
        lb.bind("<Return>",   confirm)
        dlg.protocol("WM_DELETE_WINDOW", cancel)

        # Bottom bar (row 3)
        bf = ttk.Frame(frame)
        bf.grid(row=3, column=0, sticky="ew", pady=(6, 0))
        fav_btn = ttk.Button(bf, text="☆ Add to Favorites", width=20, command=_toggle_fav)
        fav_btn.pack(side="left")
        ttk.Button(bf, text="Cancel", command=cancel).pack(side="right", padx=(4, 0))
        ttk.Button(bf, text="OK",     command=confirm).pack(side="right")

        populate()

        dlg.wait_window()
        return result[0] if result else ""

    def _pick_font_file(self, init_dir: str = "") -> str:
        """Fallback: plain file-dialog font picker (used on Windows when list picker fails)."""
        path = filedialog.askopenfilename(
            parent=self.root,
            title="Select font file",
            initialdir=init_dir or os.path.expanduser("~"),
            filetypes=[("Font files", "*.ttf *.otf *.ttc"), ("All files", "*.*")])
        if not path and sys.platform == "win32":
            win_fonts = os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts")
            if os.path.isdir(win_fonts):
                path = self._win_fonts_picker(win_fonts, self.font_var.get())
        return path

    def _win_fonts_picker(self, fonts_dir: str, current: str = "") -> str:
        exts = {".ttf", ".otf", ".ttc"}
        try:
            files = sorted(f for f in os.listdir(fonts_dir)
                           if os.path.splitext(f)[1].lower() in exts)
        except OSError:
            return ""
        if not files:
            return ""

        dlg = tk.Toplevel(self.root)
        dlg.title("Select Font")
        dlg.grab_set()
        dlg.resizable(True, True)
        result: list = []

        frame = ttk.Frame(dlg, padding=8)
        frame.pack(fill="both", expand=True)
        ttk.Label(frame, text=fonts_dir, foreground="gray").pack(anchor="w")

        lbf = ttk.Frame(frame)
        lbf.pack(fill="both", expand=True, pady=(4, 4))
        sb = ttk.Scrollbar(lbf, orient="vertical")
        lb = tk.Listbox(lbf, yscrollcommand=sb.set, width=52, height=22, selectmode="single")
        sb.config(command=lb.yview)
        sb.pack(side="right", fill="y")
        lb.pack(side="left", fill="both", expand=True)

        for fi in files:
            lb.insert("end", fi)

        if current:
            cur_name = os.path.basename(current)
            if cur_name in files:
                idx = files.index(cur_name)
                lb.selection_set(idx)
                lb.see(idx)

        def confirm():
            sel = lb.curselection()
            if sel:
                result.append(os.path.join(fonts_dir, lb.get(sel[0])))
            dlg.destroy()

        lb.bind("<Double-1>", lambda _e: confirm())

        bf = ttk.Frame(frame)
        bf.pack(fill="x")
        ttk.Button(bf, text="Cancel", command=dlg.destroy).pack(side="right", padx=(4, 0))
        ttk.Button(bf, text="OK", command=confirm).pack(side="right")

        dlg.wait_window()
        return result[0] if result else ""

    def _browse_bgphoto(self):
        path = filedialog.askopenfilename(
            parent=self.root,
            title="Select background photo",
            filetypes=[("Images", "*.jpg *.jpeg *.png *.tiff *.tif *.bmp *.webp"),
                       ("All files", "*.*")])
        if path:
            self.bgphoto_var.set(path)

    def _pick_color(self, var: tk.StringVar, swatch: tk.Label):
        try:
            init = swatch.cget("bg")
        except tk.TclError:
            init = "#808080"
        result = colorchooser.askcolor(color=init, parent=self.root)
        if result[1]:
            var.set(result[1])

    # ── Preview ───────────────────────────────────────────────────────────────

    def _build_preview(self):
        self.canvas = tk.Label(self.win, bg="black", width=PREVIEW_W, height=PREVIEW_H)
        self.canvas.pack(fill="both", expand=True)

    def _position_windows(self):
        self.root.update_idletasks()
        cw = self.root.winfo_reqwidth()
        self.root.geometry("+80+200")
        self.win.geometry(f"{PREVIEW_W}x{PREVIEW_H}+{80 + cw + 16}+200")
        self.root.lift()
        self.root.focus_force()

    # ── Zoom ──────────────────────────────────────────────────────────────────

    def _zoom(self, delta):
        new_scale = 1.0 if delta == 0 else round(max(0.7, min(2.0, self.ui_scale + delta)), 1)
        if new_scale == self.ui_scale:
            return
        t1 = self.text_widget.get("1.0", "end-1c")
        t2 = self.text2_widget.get("1.0", "end-1c")
        for w in self.root.winfo_children():
            if w is not self.win:
                w.destroy()
        self.ui_scale = new_scale
        self._build_controls()
        if t1:
            self.text_widget.insert("1.0", t1)
        if t2:
            self.text2_widget.insert("1.0", t2)
        self._update_theme_dropdown()
        self._live_state["ui_scale"] = str(new_scale)
        save_live_state(self._live_path, self._live_state)
        self._position_windows()
        self._schedule(0)

    # ── Pillow font-size fitting ───────────────────────────────────────────────
    # When a font file path is given, fit the text in Python (fast) and pass
    # --textsize=N so textimage skips the slower ImageMagick caption: auto-fit.

    def _fit_fontsize_pillow(self, text: str, max_cap: int = 0) -> int:
        font_path = self.font_var.get().strip()
        if not font_path:
            font_path = r"C:\Windows\Fonts\pala.ttf" if sys.platform == "win32" else ""
        if not font_path or not os.path.isfile(font_path):
            return 0
        try:
            img_w = int(self.width_var.get().strip()  or "1920")
            img_h = int(self.height_var.get().strip() or "1080")
        except ValueError:
            return 0
        ts = self.textscale_var.get().strip()
        tscale = int(ts) if re.fullmatch(r"\d+", ts) else 100
        target_w = int(img_w * 0.896 * tscale / 100.0)
        target_h = int(img_h * 0.741 * tscale / 100.0)
        for _side, _var in (("top",    self.reserve_top_var),    ("right",  self.reserve_right_var),
                             ("bottom", self.reserve_bottom_var), ("left",   self.reserve_left_var)):
            _pct = _var.get().strip()
            if re.fullmatch(r'\d+', _pct) and int(_pct) > 0:
                if _side in ("top", "bottom"):
                    target_h = max(1, int(target_h * (1.0 - int(_pct) / 100.0)))
                else:
                    target_w = max(1, int(target_w * (1.0 - int(_pct) / 100.0)))
        ls = self.linespacing_var.get().strip()
        linespacing = int(ls) if re.fullmatch(r"-?\d+", ls) and ls != "0" else 0
        hi = min(target_w // 2, target_h)
        if max_cap > 0:
            hi = min(hi, max_cap)
        # Split on hard newlines first, then word-wrap within each segment
        paragraphs = text.split("\n")
        lo, best = 8, 8
        while lo <= hi:
            mid = (lo + hi) // 2
            try:
                font = ImageFont.truetype(font_path, mid)
            except Exception:
                return 0
            lines = []
            for para in paragraphs:
                words = para.split()
                if not words:
                    lines.append("")
                    continue
                cur: list = []
                for word in words:
                    test = " ".join(cur + [word])
                    if cur and font.getlength(test) > target_w:
                        lines.append(" ".join(cur))
                        cur = [word]
                    else:
                        cur = cur + [word]
                if cur:
                    lines.append(" ".join(cur))
            if not lines:
                return 0
            ascent, descent = font.getmetrics()
            n = len(lines)
            total_h = n * (ascent + descent) + max(0, n - 1) * linespacing
            max_w = max((font.getlength(ln) for ln in lines), default=0)
            if max_w <= target_w and total_h <= target_h:
                best, lo = mid, mid + 1
            else:
                hi = mid - 1
        return best

    # ── Command building ──────────────────────────────────────────────────────

    def _get_text(self) -> str:
        """Return current text widget content, trailing newline stripped."""
        return self.text_widget.get("1.0", "end-1c")

    def _build_cmd(self) -> list:
        text = self._get_text()
        # Encode actual newlines as \n escape for textimage
        text_for_cmd = text.replace("\n", "\\n")

        cmd = [TEXTIMAGE, f"--text={text_for_cmd}", f"--output={TMP_JPG}"]

        mf     = self.maxtextsize_var.get().strip()
        tp_abs = self.textsize_var.get().strip()
        ts     = self.textscale_var.get().strip()
        has_fixed = bool(re.fullmatch(r'\d+', tp_abs) and int(tp_abs) > 0)
        fitted = 0
        if not has_fixed:
            max_cap = int(mf) if re.fullmatch(r'\d+', mf) and int(mf) > 0 else 0
            if text:
                fitted = self._fit_fontsize_pillow(text, max_cap)
        if fitted > 0:
            cmd.append("--maxtextsize=0")
            cmd.append(f"--textsize={fitted}")
            cmd.append("--textscale=100")
        elif has_fixed:
            cmd.append("--maxtextsize=0")
            cmd.append(f"--textsize={tp_abs}")
            cmd.append("--textscale=100")
        else:
            cmd.append(f"--maxtextsize={mf}" if re.fullmatch(r'\d+', mf) and int(mf) > 0 else "--maxtextsize=0")
            cmd.append("--textsize=0")
            cmd.append(f"--textscale={ts}" if re.fullmatch(r'\d+', ts) else "--textscale=100")

        font = self.font_var.get().strip()
        if font:
            cmd.append(f"--font={font}")

        w = self.width_var.get().strip()
        if re.fullmatch(r'\d+', w):
            cmd.append(f"--width={w}")
        h = self.height_var.get().strip()
        if re.fullmatch(r'\d+', h):
            cmd.append(f"--height={h}")

        bg = self.bg_var.get().strip()
        if bg:
            cmd.append(f"--bg={bg}")
        tc = self.textcolor_var.get().strip()
        if tc:
            cmd.append(f"--textcolor={tc}")

        photo = self.bgphoto_var.get().strip()
        if photo:
            cmd.append(f"--bgphoto={photo}")
            dim = self.dim_var.get().strip()
            if re.fullmatch(r'\d+', dim):
                cmd.append(f"--dim={dim}")

        ts = self.textshadow_var.get().strip()
        if re.fullmatch(r'\d+', ts) and int(ts) > 0:
            cmd.append(f"--textshadow={ts}")
        else:
            cmd.append("--no-textshadow")
        cmd.append(f"--shadowmethod={self.shadowmethod_var.get()}")

        tol = self.textoutline_var.get().strip()
        if re.fullmatch(r'\d+', tol) and int(tol) > 0:
            cmd.append(f"--textoutline={tol}")
            toc = self.textoutlinecolor_var.get().strip()
            if toc and toc != "black":
                cmd.append(f"--textoutlinecolor={toc}")
        else:
            cmd.append("--no-textoutline")

        tp = self.textpanel_var.get().strip()
        if re.fullmatch(r'\d+', tp):
            cmd.append(f"--textpanel={tp}")
        cmd.append("--textpanelrounded" if self.panelrounded_var.get() else "--no-textpanelrounded")
        tpc = self.textpanelcolor_var.get().strip()
        if tpc and tpc != "black":
            cmd.append(f"--textpanelcolor={tpc}")

        ls = self.linespacing_var.get().strip()
        if re.fullmatch(r'-?\d+', ls) and ls != "0":
            cmd.append(f"--linespacing={ls}")

        toffy = self.textoffy_var.get().strip()
        if re.fullmatch(r'-?\d+', toffy) and toffy != "0":
            cmd.append(f"--textoffy={toffy}")

        for _side, _var in (("top",    self.reserve_top_var),    ("right",  self.reserve_right_var),
                             ("bottom", self.reserve_bottom_var), ("left",   self.reserve_left_var)):
            _pct = _var.get().strip()
            if re.fullmatch(r'\d+', _pct) and int(_pct) > 0:
                cmd.append(f"--reserve={_side},{_pct}")

        text2 = self.text2_widget.get("1.0", "end-1c").strip()
        if text2:
            cmd.append(f"--text2={text2.replace(chr(10), chr(92) + 'n')}")
            t2gap = self.text2gap_var.get().strip()
            if re.fullmatch(r'\d+', t2gap) and t2gap != "40":
                cmd.append(f"--text2gap={t2gap}")
            t2color = self.text2color_var.get().strip()
            if t2color:
                cmd.append(f"--text2color={t2color}")
            t2font = self.text2font_var.get().strip()
            if t2font:
                cmd.append(f"--text2font={t2font}")
            t2outline = self.text2outline_var.get().strip()
            if t2outline:
                cmd.append(f"--text2outline={t2outline}")
            t2outlinecolor = self.text2outlinecolor_var.get().strip()
            if t2outlinecolor:
                cmd.append(f"--text2outlinecolor={t2outlinecolor}")
            t2shadow = self.text2shadow_var.get().strip()
            if t2shadow:
                cmd.append(f"--text2shadow={t2shadow}")
            t2shadowmethod = self.text2shadowmethod_var.get().strip()
            if t2shadowmethod and t2shadowmethod != "1":
                cmd.append(f"--text2shadowmethod={t2shadowmethod}")

        return cmd

    def _copy_cmd(self):
        cmd = self._build_cmd()
        # Replace tmp output with a descriptive filename
        text = self._get_text()
        words = re.sub(r'[^\w\s]', '', text).split()
        default_out = "_".join(words[:4]) + ".jpg" if words else "text_image.jpg"
        cmd = [f"--output={default_out}" if a.startswith("--output=") else a for a in cmd]
        self.root.clipboard_clear()
        self.root.clipboard_append(shlex.join(cmd))
        self.status_var.set("Command copied to clipboard")

    def _save_image(self):
        if not os.path.exists(TMP_JPG):
            return
        text = self._get_text()
        words = re.sub(r'[^\w\s]', '', text).split()
        default_name = "_".join(words[:4]) + ".jpg" if words else "text_image.jpg"
        path = filedialog.asksaveasfilename(
            parent=self.root,
            title="Save Image",
            initialfile=default_name,
            defaultextension=".jpg",
            filetypes=[("JPEG", "*.jpg *.jpeg"), ("All files", "*.*")])
        if path:
            shutil.copy2(TMP_JPG, path)

    def _on_half_size_toggle(self):
        self._live_state["half_size"] = "yes" if self.half_size_var.get() else "no"
        save_live_state(self._live_path, self._live_state)
        self._redisplay()

    # ── Rendering ─────────────────────────────────────────────────────────────

    def _schedule(self, delay_ms: int = 400):
        if not self._live_active:
            return
        if self._after_id is not None:
            self.root.after_cancel(self._after_id)
        self._after_id = self.root.after(delay_ms, self._render)

    def _toggle_live(self):
        self._live_active = not self._live_active
        self.live_btn.config(text="Pause" if self._live_active else "Go Live")
        if self._live_active:
            self._schedule(0)

    def _render(self):
        self._after_id = None
        text = self._get_text()
        if not text.strip():
            return
        proc = self._proc
        if proc is not None:
            try:
                proc.kill()
                proc.wait()
            except Exception:
                pass
        self.status_var.set("Rendering…")
        self.root.update_idletasks()
        self._render_gen += 1
        gen = self._render_gen
        cmd = self._build_cmd()
        threading.Thread(target=self._run_proc, args=(cmd, gen), daemon=True).start()

    def _run_proc(self, cmd: list, gen: int):
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self._proc = proc
        stdout, stderr = proc.communicate()
        self._proc = None
        result = subprocess.CompletedProcess(cmd, proc.returncode,
                                             stdout.decode(errors="replace"),
                                             stderr.decode(errors="replace"))
        self.root.after(0, self._render_done, result, gen)

    def _append_log(self, stderr: str):
        import datetime
        try:
            with open(self._log_path, "a", encoding="utf-8") as fh:
                fh.write(f"[{datetime.datetime.now():%Y-%m-%d %H:%M:%S}]\n")
                fh.write(stderr.rstrip())
                fh.write("\n\n")
        except OSError:
            pass

    def _render_done(self, result, gen: int):
        if gen != self._render_gen:
            return
        filtered = "\n".join(
            l for l in result.stderr.splitlines()
            if not l.startswith("Saved to /var/")
        )
        if filtered.strip():
            self._append_log(filtered)
        if result.returncode != 0:
            lines = (result.stderr or result.stdout).strip().splitlines()
            msg = lines[0] if lines else "textimage error"
            self.status_var.set(msg)
            return
        try:
            img = Image.open(TMP_JPG)
            self._display_image(img)
            text = self._get_text()
            preview = text.replace("\n", " ↵ ")[:60]
            self.status_var.set(preview)
        except Exception as exc:
            self.status_var.set(str(exc))

    def _display_image(self, img: "Image.Image"):
        if self.half_size_var.get():
            disp_w = max(1, img.width  // 2)
            disp_h = max(1, img.height // 2)
            img = img.resize((disp_w, disp_h), Image.LANCZOS)
        else:
            disp_w, disp_h = img.width, img.height
        self._photo = ImageTk.PhotoImage(img)
        self.canvas.config(image=self._photo, width=disp_w, height=disp_h)
        self.win.geometry(f"{disp_w}x{disp_h}")

    def _redisplay(self):
        if not os.path.exists(TMP_JPG):
            return
        try:
            self._display_image(Image.open(TMP_JPG))
        except Exception:
            pass

    # ── Lifecycle ─────────────────────────────────────────────────────────────

    def _quit(self):
        try:
            os.unlink(TMP_JPG)
        except FileNotFoundError:
            pass
        self.root.destroy()

    def run(self):
        self.root.mainloop()


if __name__ == "__main__":
    TextImageView().run()
