#!/usr/bin/env python3
"""bvilive — two-window live preview for bvi"""

import io
import tkinter as tk
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

BVI         = str(Path(__file__).parent / ("bvi.exe" if sys.platform == "win32" else "bvi"))
BIBLES      = {"KJV": "BibleKJV.txt", "BSB": "BibleBSB.txt", "WEB": "BibleWEB.txt"}
_tmp_fd, TMP_JPG = tempfile.mkstemp(suffix=".jpg", prefix="bvilive_")
os.close(_tmp_fd)
CONFIG_FILE = ".verselumen"

PREVIEW_W = 960   # display width  (half of default 1920)
PREVIEW_H = 540   # display height (half of default 1080)


def load_bvi_config(path: str = CONFIG_FILE) -> dict:
    """Parse the [bvi] section of .verselumen, mirroring bvi.cpp's loadConfig()."""
    cfg: dict = {}
    try:
        with open(path, encoding="utf-8-sig") as fh:
            in_section = False
            for line in fh:
                line = line.rstrip("\r\n").lstrip(" \t")
                if line.startswith("["):
                    end = line.find("]")
                    sec = line[1:end] if end != -1 else ""
                    in_section = (sec == "bvi")
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
    ("bv",               "version_var",          "str",  "KJV"),
    ("font",             "font_var",              "str",  ""),
    ("citefont",         "citefont_var",          "str",  ""),
    ("bg",               "bg_var",                "str",  "black"),
    ("textcolor",        "textcolor_var",         "str",  "white"),
    ("citecolor",        "citecolor_var",         "str",  "gray60"),
    ("bgphoto",          "bgphoto_var",           "str",  ""),
    ("dim",              "dim_var",               "str",  "50"),
    ("width",            "width_var",             "str",  "1920"),
    ("height",           "height_var",            "str",  "1080"),
    ("citestyle",        "citestyle_var",         "str",  "dash"),
    ("citeplacement",    "citeplacement_var",     "str",  "bottom"),
    ("citeshadow",       "citeshadow_var",         "str",  "0"),
    ("citealign",        "citealign_var",          "str",  "center"),
    ("citepanel",        "citepanel_var",          "str",  "independent"),
    ("textoffy",         "textoffy_var",           "str",  "0"),
    ("citeoffy",         "citeoffy_var",           "str",  "0"),
    ("citescale",        "citescale_var",         "str",  ""),
    ("maxtextsize",      "maxtextsize_var",       "str",  ""),
    ("textsize",         "textsize_var",          "str",  ""),
    ("textscale",        "textscale_var",         "str",  ""),
    ("textpanel",        "textpanel_var",         "str",  ""),
    ("textpanelcolor",   "textpanelcolor_var",    "str",  "black"),
    ("quotes",           "quotes_var",            "bool", "no"),
    ("textshadow",       "textshadow_var",        "str",  "0"),
    ("shadowmethod",     "shadowmethod_var",      "str",  "1"),
    ("textoutline",      "textoutline_var",       "str",  "0"),
    ("textoutlinecolor", "textoutlinecolor_var",  "str",  "black"),
    ("linespacing",      "linespacing_var",       "str",  "0"),
    ("reservetop",       "reserve_top_var",       "str",  "0"),
    ("reserveright",     "reserve_right_var",     "str",  "0"),
    ("reservebottom",    "reserve_bottom_var",    "str",  "0"),
    ("reserveleft",      "reserve_left_var",      "str",  "0"),
    ("textpanelrounded", "panelrounded_var",      "bool", "no"),
    ("citebibleversion", "citebibleversion_var",  "bool", "yes"),
    ("customtext",         "customtext_var",       "str",  ""),
    ("customtext_enabled", "customtext_enabled",   "bool", "no"),
]


def find_bvilive_path() -> str:
    """Return the .verselumen path to use: local if present, else $HOME if present, else local."""
    local = CONFIG_FILE
    if os.path.exists(local):
        return local
    home = os.path.join(os.path.expanduser("~"), CONFIG_FILE)
    if os.path.exists(home):
        return home
    return local


def load_bvilive_state(path: str) -> dict:
    """Parse [bvilive] section of .verselumen → {last_ref, default_theme, themes:{name:{key:val}}}."""
    state: dict = {"last_ref": "", "default_theme": "", "themes": {}}
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
                        in_section = (sec == "bvilive")
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
                    if key in ("last_ref", "default_theme", "half_size", "font_favorites"):
                        state[key] = val
                else:
                    state["themes"][current][key] = val
    except FileNotFoundError:
        pass
    return state


def save_bvilive_state(path: str, state: dict):
    """Write [bvilive] section of .verselumen, preserving all other sections."""
    new_lines: list[str] = []
    if state.get("last_ref"):
        new_lines.append(f"last_ref = {state['last_ref']}\n")
    if state.get("default_theme"):
        new_lines.append(f"default_theme = {state['default_theme']}\n")
    if "half_size" in state:
        new_lines.append(f"half_size = {state['half_size']}\n")
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
                        if sec == "bvilive":
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
        fh.write("[bvilive]\n")
        fh.writelines(new_lines)
        if after:
            fh.write("\n")
            fh.writelines(after)


def _find_bible_path(bible_dir: Path, filename: str) -> "Path | None":
    """Mirrors bvi.cpp's Bible-file search: local dir → Bible/ subdir → USERPROFILE/HOME."""
    candidates = [
        bible_dir / filename,
        bible_dir / "Bible" / filename,
        Path.home() / filename,
    ]
    for p in candidates:
        if p.exists():
            return p
    return None


def load_verse_index(bible_dir: Path, filename: str) -> "tuple[dict, dict]":
    """Return ({book: {chapter: max_verse}}, {ref: text}) from a Bible text file in one pass."""
    index: dict = {}
    verses: dict = {}
    path = _find_bible_path(bible_dir, filename)
    if path is None:
        return index, verses
    with open(path, encoding="utf-8-sig") as fh:
        for line in fh:
            line = line.rstrip("\n\r")
            if "\t" not in line:
                continue
            ref, text = line.split("\t", 1)
            verses[ref] = text
            m = re.match(r'^(.+?)\s+(\d+):(\d+)$', ref)
            if not m:
                continue
            book, ch, vs = m.group(1), int(m.group(2)), int(m.group(3))
            index.setdefault(book, {}).setdefault(ch, 0)
            if vs > index[book][ch]:
                index[book][ch] = vs
    return index, verses


class BviView:
    def __init__(self):
        self.bible_dir  = Path(BVI).parent
        self.verse_index: dict = {}      # loaded per version
        self._index_cache: dict = {}     # version → index
        self._verse_text_cache: dict = {}  # version → {ref: text}

        # ── Control window ────────────────────────────────────────────────────
        self.root = tk.Tk()
        self.root.title("bvilive — Controls")
        self.root.resizable(False, False)
        self.root.protocol("WM_DELETE_WINDOW", self._quit)

        # ── Preview window ────────────────────────────────────────────────────
        self.win = tk.Toplevel(self.root)
        self.win.title("bvilive — Preview")
        self.win.resizable(True, True)
        self.win.protocol("WM_DELETE_WINDOW", self._quit)

        # ── State ─────────────────────────────────────────────────────────────
        self._after_id     = None
        self._render_gen   = 0        # incremented each render; stale completions are discarded
        self._bvi_proc     = None     # running bvi Popen, killed when a new render starts
        self._render_lock  = threading.Lock()
        self._photo        = None     # keep ImageTk ref alive
        self._last_ref     = ""
        self._live_active  = True
        self._browse_ref_geometry: str | None = None

        cfg = load_bvi_config()

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
            self._bvilive_path = local_cfg if use_local else home_cfg
        else:
            self._bvilive_path = find_bvilive_path()
        self._bvilive_state = load_bvilive_state(self._bvilive_path)

        def _cfg(key, default=""):
            return cfg.get(key, default)

        maxtextsize  = int(_cfg("maxtextsize", "0"))
        textsize_val = int(_cfg("textsize",    "0"))
        textscale    = int(_cfg("textscale",   "100"))
        citescale    = int(_cfg("citescale",   "100"))

        last_ref = self._bvilive_state.get("last_ref", "") or "Luke 1:1"
        self.ref_var        = tk.StringVar(value=last_ref)
        self.version_var    = tk.StringVar(value=_cfg("bv", "KJV"))
        self.maxtextsize_var    = tk.StringVar(value=str(maxtextsize)  if maxtextsize  != 0   else "")
        self.textsize_var       = tk.StringVar(value=str(textsize_val) if textsize_val != 0   else "")
        self.textscale_var  = tk.StringVar(value=str(textscale) if textscale != 100 else "")
        self.font_var       = tk.StringVar(value=_cfg("font",     ""))
        self.citefont_var   = tk.StringVar(value=_cfg("citefont", ""))
        self.citescale_var  = tk.StringVar(value=str(citescale) if citescale != 100 else "")
        self.width_var      = tk.StringVar(value=_cfg("width",  "1920"))
        self.height_var     = tk.StringVar(value=_cfg("height", "1080"))
        self.citestyle_var     = tk.StringVar(value=_cfg("citestyle",     "dash"))
        self.citeplacement_var = tk.StringVar(value=_cfg("citeplacement", "bottom"))
        _cs = _cfg("citeshadow", "0")
        self.citeshadow_var    = tk.StringVar(value="5" if _cs == "yes" else "0" if _cs == "no" else _cs)
        self.citealign_var     = tk.StringVar(value=_cfg("citealign", "center"))
        self.citepanel_var     = tk.StringVar(value=_cfg("citepanel", "independent"))
        self.textoffy_var      = tk.StringVar(value=_cfg("textoffy", "0"))
        self.citeoffy_var      = tk.StringVar(value=_cfg("citeoffy", "0"))
        self.quotes_var     = tk.BooleanVar(value=(_cfg("quotes", "no") == "yes"))
        self.bg_var         = tk.StringVar(value=_cfg("bg",        "black"))
        self.textcolor_var  = tk.StringVar(value=_cfg("textcolor", "white"))
        self.citecolor_var  = tk.StringVar(value=_cfg("citecolor", "gray60"))
        self.bgphoto_var    = tk.StringVar(value=_cfg("bgphoto", ""))
        self.dim_var        = tk.StringVar(value=_cfg("dim", "50"))
        _ts = _cfg("textshadow", "0")
        self.textshadow_var    = tk.StringVar(value="5" if _ts == "yes" else "0" if _ts == "no" else _ts)
        self.shadowmethod_var  = tk.StringVar(value=_cfg("shadowmethod", "1"))
        self.textoutline_var      = tk.StringVar(value=_cfg("textoutline", "0"))
        self.textoutlinecolor_var = tk.StringVar(value=_cfg("textoutlinecolor", "black"))
        self.linespacing_var   = tk.StringVar(value=_cfg("linespacing", "0"))
        self.reserve_top_var    = tk.StringVar(value=_cfg("reservetop",    "0"))
        self.reserve_right_var  = tk.StringVar(value=_cfg("reserveright",  "0"))
        self.reserve_bottom_var = tk.StringVar(value=_cfg("reservebottom", "0"))
        self.reserve_left_var   = tk.StringVar(value=_cfg("reserveleft",   "0"))
        self.textpanel_var     = tk.StringVar(value=_cfg("textpanel", ""))
        self.textpanelcolor_var = tk.StringVar(value=_cfg("textpanelcolor", "black"))
        self.panelrounded_var  = tk.BooleanVar(value=_cfg("textpanelrounded", "no") == "yes")
        self.citebibleversion_var = tk.BooleanVar(
            value=_cfg("citebibleversion", "yes") not in ("no", "false"))
        half_size_saved = self._bvilive_state.get("half_size", "yes")
        self.half_size_var      = tk.BooleanVar(value=half_size_saved != "no")
        self.customtext_var     = tk.StringVar(value=_cfg("customtext", ""))
        self.customtext_enabled = tk.BooleanVar(value=_cfg("customtext_enabled", "no") == "yes")
        self.status_var     = tk.StringVar(value="Ready")

        self._build_controls()
        self._build_preview()
        if sys.platform == "linux":
            self.root.after(100, self._position_windows)
        else:
            self._position_windows()
        self._load_index(self.version_var.get())
        # Apply default theme (if any) — overrides .bvi values; also triggers render
        default_name = self._bvilive_state.get("default_theme", "")
        if default_name and default_name in self._bvilive_state["themes"]:
            self._apply_theme(default_name)
            self.theme_var.set(self._theme_display(default_name))
        else:
            self._schedule(0)

    # ── UI construction ───────────────────────────────────────────────────────

    def _build_controls(self):
        f = self.root
        pad = dict(padx=8, pady=3)
        f.columnconfigure(1, minsize=110)
        f.columnconfigure(2, minsize=110)

        # Prev / Next nav buttons
        nav = tk.Frame(f)
        nav.grid(row=0, column=0, columnspan=4, pady=2)
        tk.Button(nav, text="◀◀ Chap",  width=8, command=lambda: self._step_chapter(-1)).pack(side="left", padx=2)
        tk.Button(nav, text="◀ Verse",  width=8, command=lambda: self._step(-1)).pack(side="left", padx=2)
        tk.Button(nav, text="Verse ▶",  width=8, command=lambda: self._step(+1)).pack(side="left", padx=2)
        tk.Button(nav, text="Chap ▶▶",  width=8, command=lambda: self._step_chapter(+1)).pack(side="left", padx=2)

        # Reference
        tk.Label(f, text="Reference:").grid(row=1, column=0, sticky="e", **pad)
        ref_e = tk.Entry(f, textvariable=self.ref_var, width=30, font=("", 13))
        ref_e.grid(row=1, column=1, columnspan=2, sticky="ew", **pad)
        ref_e.bind("<Return>",    lambda _: self._schedule(0))
        ref_e.bind("<Up>",        lambda _: self._step(-1))
        ref_e.bind("<Down>",      lambda _: self._step(+1))
        ref_e.bind("<Prior>",     lambda _: self._step_chapter(-1))  # Page Up
        ref_e.bind("<Next>",      lambda _: self._step_chapter(+1))  # Page Down
        self.ref_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Button(f, text="…", padx=2,
                  command=self._browse_ref).grid(row=1, column=3, sticky="w", padx=(0, 6), pady=3)

        # ← / → navigate verses globally; skip when an Entry has keyboard focus
        self.root.bind_all("<Left>",  self._on_key_left)
        self.root.bind_all("<Right>", self._on_key_right)

        # Custom text
        self.customtext_cb = tk.Checkbutton(f, text="Custom text:", variable=self.customtext_enabled,
                                            command=self._on_customtext_toggle)
        self.customtext_cb.grid(row=2, column=0, sticky="e", **pad)
        self.customtext_entry = tk.Entry(f, textvariable=self.customtext_var, width=30)
        self.customtext_entry.grid(row=2, column=1, columnspan=3, sticky="ew", **pad)
        self.customtext_var.trace_add("write", lambda *_: self._schedule(400))
        self._update_customtext_state()

        # Version
        tk.Label(f, text="Version:").grid(row=3, column=0, sticky="e", **pad)
        vf = tk.Frame(f)
        vf.grid(row=3, column=1, columnspan=3, sticky="w")
        for v in ("KJV", "BSB", "WEB"):
            tk.Radiobutton(vf, text=v, variable=self.version_var, value=v,
                           command=self._on_version_change).pack(side="left")

        # Max text size (cap) / Text scale %
        tk.Label(f, text="Max text pt:").grid(row=4, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.maxtextsize_var, width=6).grid(row=4, column=1, sticky="w", **pad)
        self.maxtextsize_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Label(f, text="Text scale %:").grid(row=4, column=2, sticky="e", **pad)
        tk.Entry(f, textvariable=self.textscale_var, width=6).grid(row=4, column=3, sticky="w", **pad)
        self.textscale_var.trace_add("write", lambda *_: self._schedule(400))

        # Text pt (absolute) / Text Y offset
        tk.Label(f, text="Text pt:").grid(row=5, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.textsize_var, width=6).grid(row=5, column=1, sticky="w", **pad)
        self.textsize_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Label(f, text="Text off Y:").grid(row=5, column=2, sticky="e", **pad)
        tk.Entry(f, textvariable=self.textoffy_var, width=5).grid(row=5, column=3, sticky="w", **pad)
        self.textoffy_var.trace_add("write", lambda *_: self._schedule(400))

        # Font
        tk.Label(f, text="Font:").grid(row=6, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.font_var, width=28).grid(row=6, column=1, columnspan=2, sticky="ew", **pad)
        self.font_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Button(f, text="…", padx=2,
                  command=self._browse_font).grid(row=6, column=3, sticky="w", padx=(0, 6), pady=3)

        # Width / Height
        tk.Label(f, text="Width:").grid(row=7, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.width_var, width=6).grid(row=7, column=1, sticky="w", **pad)
        self.width_var.trace_add("write", lambda *_: self._schedule(600))
        tk.Label(f, text="Height:").grid(row=7, column=2, sticky="e", **pad)
        tk.Entry(f, textvariable=self.height_var, width=6).grid(row=7, column=3, sticky="w", **pad)
        self.height_var.trace_add("write", lambda *_: self._schedule(600))

        # Cite style / Quotes / Cite ver.
        tk.Label(f, text="Cite style:").grid(row=8, column=0, sticky="e", **pad)
        ttk.Combobox(f, textvariable=self.citestyle_var, width=8,
                     values=("dash", "parens", "plain", "none"),
                     state="readonly").grid(row=8, column=1, sticky="w", **pad)
        self.citestyle_var.trace_add("write", lambda *_: self._schedule(0))
        tk.Checkbutton(f, text="Quotes", variable=self.quotes_var,
                       command=lambda: self._schedule(0)).grid(row=8, column=2, sticky="w", **pad)
        tk.Checkbutton(f, text="Cite ver.", variable=self.citebibleversion_var,
                       command=lambda: self._schedule(0)).grid(row=8, column=3, sticky="w", **pad)

        # Cite placement / Cite shadow
        tk.Label(f, text="Cite placement:").grid(row=9, column=0, sticky="e", **pad)
        cp_frame = tk.Frame(f)
        cp_frame.grid(row=9, column=1, columnspan=1, sticky="w")
        for val in ("bottom", "below"):
            tk.Radiobutton(cp_frame, text=val, variable=self.citeplacement_var, value=val,
                           command=lambda: self._schedule(0)).pack(side="left")
        tk.Label(f, text="Cite shadow (0-10):").grid(row=9, column=2, sticky="e", **pad)
        tk.Entry(f, textvariable=self.citeshadow_var, width=3).grid(row=9, column=3, sticky="w", **pad)
        self.citeshadow_var.trace_add("write", lambda *_: self._schedule(400))

        # Cite font / Cite scale %
        tk.Label(f, text="Cite font:").grid(row=10, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.citefont_var, width=28).grid(row=10, column=1, columnspan=2, sticky="ew", **pad)
        self.citefont_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Button(f, text="…", padx=2,
                  command=self._browse_citefont).grid(row=10, column=3, sticky="w", padx=(0, 6), pady=3)

        tk.Label(f, text="Cite scale %:").grid(row=11, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.citescale_var, width=6).grid(row=11, column=1, sticky="w", **pad)
        self.citescale_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Label(f, text="Cite align:").grid(row=11, column=2, sticky="e", **pad)
        ttk.Combobox(f, textvariable=self.citealign_var, width=7,
                     values=("center", "left", "right"),
                     state="readonly").grid(row=11, column=3, sticky="w", **pad)
        self.citealign_var.trace_add("write", lambda *_: self._schedule(0))

        # Cite panel mode / Cite Y offset
        tk.Label(f, text="Cite panel:").grid(row=12, column=0, sticky="e", **pad)
        ttk.Combobox(f, textvariable=self.citepanel_var, width=11,
                     values=("independent", "coverbottom", "none"),
                     state="readonly").grid(row=12, column=1, sticky="w", **pad)
        self.citepanel_var.trace_add("write", lambda *_: self._schedule(0))
        tk.Label(f, text="Cite off Y:").grid(row=12, column=2, sticky="e", **pad)
        tk.Entry(f, textvariable=self.citeoffy_var, width=5).grid(row=12, column=3, sticky="w", **pad)
        self.citeoffy_var.trace_add("write", lambda *_: self._schedule(400))

        # Colors
        self._make_color_row(f, "BG:",             self.bg_var,             13)
        self._make_color_row(f, "Text color:",      self.textcolor_var,      14)
        self._make_color_row(f, "Text panel color:", self.textpanelcolor_var, 15)
        self._make_color_row(f, "Cite color:",      self.citecolor_var,      16)

        # BG Photo
        tk.Label(f, text="BG photo:").grid(row=17, column=0, sticky="e", **pad)
        photo_entry = tk.Entry(f, textvariable=self.bgphoto_var, width=28)
        photo_entry.grid(row=17, column=1, columnspan=2, sticky="ew", **pad)
        self.bgphoto_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Button(f, text="…", padx=2,
                  command=self._browse_bgphoto).grid(row=17, column=3, sticky="w", padx=(0, 6), pady=3)

        # Dim / Text shadow + shadow method (merged row)
        tk.Label(f, text="Dim %:").grid(row=18, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.dim_var, width=5).grid(row=18, column=1, sticky="w", **pad)
        self.dim_var.trace_add("write", lambda *_: self._schedule(400))
        _shd_frame = tk.Frame(f)
        _shd_frame.grid(row=18, column=2, columnspan=2, sticky="w", padx=6, pady=3)
        tk.Label(_shd_frame, text="Shadow (0-10):").pack(side="left")
        tk.Entry(_shd_frame, textvariable=self.textshadow_var, width=3).pack(side="left", padx=(2, 6))
        self.textshadow_var.trace_add("write", lambda *_: self._schedule(400))
        _SM_OPTS = ["1 – Soft (Gaussian)", "2 – Hard (copy)"]
        self._sm_cb = ttk.Combobox(_shd_frame, values=_SM_OPTS, state="readonly", width=17)
        self._sm_cb.pack(side="left")

        def _sync_sm_cb(*_):
            v = self.shadowmethod_var.get()
            idx = 0 if v == "1" else 1
            self._sm_cb.current(idx)

        self.shadowmethod_var.trace_add("write", _sync_sm_cb)
        _sync_sm_cb()
        self._sm_cb.bind("<<ComboboxSelected>>",
                         lambda _: (self.shadowmethod_var.set(self._sm_cb.get()[0]),
                                    self._schedule(0)))

        # Text outline
        tk.Label(f, text="Text outline px:").grid(row=20, column=0, sticky="e", **pad)
        tk.Entry(f, textvariable=self.textoutline_var, width=3).grid(row=20, column=1, sticky="w", **pad)
        self.textoutline_var.trace_add("write", lambda *_: self._schedule(400))
        self._make_color_row(f, "Outline color:", self.textoutlinecolor_var, 20, col_start=2)

        # Text panel opacity + rounded, line spacing
        tk.Label(f, text="Text panel %:").grid(row=21, column=0, sticky="e", **pad)
        tp_frame = tk.Frame(f)
        tp_frame.grid(row=21, column=1, sticky="w", **pad)
        tk.Entry(tp_frame, textvariable=self.textpanel_var, width=5).pack(side="left")
        tk.Checkbutton(tp_frame, text="Rounded", variable=self.panelrounded_var,
                       command=lambda: self._schedule(0)).pack(side="left", padx=(4, 0))
        self.textpanel_var.trace_add("write", lambda *_: self._schedule(400))
        tk.Label(f, text="Line spacing:").grid(row=21, column=2, sticky="e", **pad)
        tk.Entry(f, textvariable=self.linespacing_var, width=5).grid(row=21, column=3, sticky="w", **pad)
        self.linespacing_var.trace_add("write", lambda *_: self._schedule(400))

        # Reserve %
        tk.Label(f, text="Reserve %:").grid(row=22, column=0, sticky="e", **pad)
        res_frame = tk.Frame(f)
        res_frame.grid(row=22, column=1, columnspan=3, sticky="w", padx=6, pady=3)
        for _lbl, _var in (("T", self.reserve_top_var), ("R", self.reserve_right_var),
                           ("B", self.reserve_bottom_var), ("L", self.reserve_left_var)):
            tk.Label(res_frame, text=f"{_lbl}:").pack(side="left")
            tk.Entry(res_frame, textvariable=_var, width=4).pack(side="left", padx=(0, 8))
            _var.trace_add("write", lambda *_: self._schedule(400))

        # Themes
        self.theme_var = tk.StringVar()
        tk.Label(f, text="Theme:").grid(row=23, column=0, sticky="e", **pad)
        self.theme_cb = ttk.Combobox(f, textvariable=self.theme_var, state="readonly", width=18)
        self.theme_cb.grid(row=23, column=1, sticky="ew", **pad)
        self.theme_cb.bind("<<ComboboxSelected>>", lambda _: self._on_theme_select())
        tbf = tk.Frame(f)
        tbf.grid(row=23, column=2, columnspan=2, sticky="w", padx=(4, 6), pady=3)
        tk.Button(tbf, text="Save…",   padx=3, command=self._save_theme_dialog).pack(side="left", padx=(0, 3))
        tk.Button(tbf, text="Delete",  padx=3, command=self._delete_theme).pack(side="left", padx=(0, 3))
        tk.Button(tbf, text="Default", padx=3, command=self._make_default_theme).pack(side="left")
        self._update_theme_dropdown()

        # Preview size / Play-Pause / Copy bvi / Save image
        tk.Checkbutton(f, text="Preview at half size", variable=self.half_size_var,
                       command=self._on_half_size_toggle).grid(row=24, column=0, columnspan=2, sticky="w", **pad)
        bf = tk.Frame(f)
        bf.grid(row=24, column=2, columnspan=2, sticky="e", padx=(0, 8), pady=3)
        self.live_btn = tk.Button(bf, text="Pause", width=6, command=self._toggle_live)
        self.live_btn.pack(side="left", padx=(0, 4))
        tk.Button(bf, text="Copy bvi", command=self._copy_bvi_cmd).pack(side="left", padx=(0, 4))
        tk.Button(bf, text="Save Image…", command=self._save_image).pack(side="left")

        # Status
        tk.Label(f, textvariable=self.status_var, fg="gray45",
                 anchor="w", width=46).grid(row=25, column=0, columnspan=4, **pad)

    def _make_color_row(self, parent, label: str, var: tk.StringVar, row: int, col_start: int = 0):
        pad = dict(padx=6, pady=3)
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
        default = self._bvilive_state.get("default_theme", "")
        return f"{name} (default)" if name == default else name

    def _theme_from_display(self, display: str) -> str:
        if display.endswith(" (default)"):
            return display[:-len(" (default)")]
        return display

    def _update_theme_dropdown(self):
        default = self._bvilive_state.get("default_theme", "")
        names = sorted(self._bvilive_state["themes"])
        values = [self._theme_display(n) for n in names]
        self.theme_cb["values"] = values
        # Re-apply display string to current selection (default marker may have changed)
        bare = self._theme_from_display(self.theme_var.get())
        if bare in self._bvilive_state["themes"]:
            self.theme_var.set(self._theme_display(bare))
        elif not bare:
            self.theme_var.set("")

    def _apply_theme(self, name: str):
        theme = self._bvilive_state["themes"][name]
        for key, attr, typ, default in THEME_KEYS:
            val = theme.get(key, default)
            var = getattr(self, attr)
            if typ == "bool":
                var.set(val in ("yes", "true", "1"))
            elif key in ("textshadow", "citeshadow"):
                var.set("5" if val == "yes" else "0" if val in ("no", "") else val)
            else:
                var.set(val)

    def _collect_theme(self) -> dict:
        theme = {}
        for key, attr, typ, _ in THEME_KEYS:
            var = getattr(self, attr)
            theme[key] = ("yes" if var.get() else "no") if typ == "bool" else var.get()
        return theme

    def _on_theme_select(self):
        name = self._theme_from_display(self.theme_var.get())
        if name and name in self._bvilive_state["themes"]:
            self._apply_theme(name)
            self._schedule(0)

    def _save_theme_dialog(self):
        initial = self._theme_from_display(self.theme_var.get()) or "My Theme"
        name = simpledialog.askstring("Save Theme", "Theme name:", initialvalue=initial, parent=self.root)
        if not name or not name.strip():
            return
        name = name.strip()
        self._bvilive_state["themes"][name] = self._collect_theme()
        save_bvilive_state(self._bvilive_path, self._bvilive_state)
        self._update_theme_dropdown()
        self.theme_var.set(self._theme_display(name))

    def _delete_theme(self):
        name = self._theme_from_display(self.theme_var.get())
        if not name or name not in self._bvilive_state["themes"]:
            return
        if not messagebox.askyesno("Delete Theme", f"Delete theme \"{name}\"?", parent=self.root):
            return
        del self._bvilive_state["themes"][name]
        if self._bvilive_state.get("default_theme") == name:
            self._bvilive_state["default_theme"] = ""
        save_bvilive_state(self._bvilive_path, self._bvilive_state)
        self._update_theme_dropdown()
        self.theme_var.set("")

    def _make_default_theme(self):
        name = self._theme_from_display(self.theme_var.get())
        if not name or name not in self._bvilive_state["themes"]:
            return
        self._bvilive_state["default_theme"] = name
        save_bvilive_state(self._bvilive_path, self._bvilive_state)
        self._update_theme_dropdown()

    def _copy_bvi_cmd(self):
        cmd = self._build_cmd()
        ref = self.ref_var.get().strip()
        default_out = (re.sub(r'[^\w]+', '_', ref).strip('_') + ".jpg") if ref else "bvilive_output.jpg"
        cmd = [f"--output={default_out}" if a.startswith("--output=") else a for a in cmd]
        # Use Windows double-quote style on Windows (shlex.join uses POSIX single-quotes
        # which are literal characters in cmd.exe and cause arguments to be misread).
        if sys.platform == "win32":
            text = subprocess.list2cmdline(cmd)
        else:
            text = shlex.join(cmd)
        self.root.clipboard_clear()
        self.root.clipboard_append(text)
        self.status_var.set("bvi command copied to clipboard")

    def _save_image(self):
        if not os.path.exists(TMP_JPG):
            return
        ref = self.ref_var.get().strip()
        default_name = (re.sub(r'[^\w]+', '_', ref).strip('_') + ".jpg") if ref else "bvilive_output.jpg"
        path = filedialog.asksaveasfilename(
            parent=self.root,
            title="Save Image",
            initialfile=default_name,
            defaultextension=".jpg",
            filetypes=[("JPEG", "*.jpg *.jpeg"), ("All files", "*.*")])
        if path:
            shutil.copy2(TMP_JPG, path)

    def _update_customtext_state(self):
        self.customtext_entry.config(state="normal")

    def _on_customtext_toggle(self):
        self._update_customtext_state()
        self._schedule(0)

    def _on_half_size_toggle(self):
        self._bvilive_state["half_size"] = "yes" if self.half_size_var.get() else "no"
        save_bvilive_state(self._bvilive_path, self._bvilive_state)
        self._redisplay()

    def _save_last_ref(self, ref: str):
        if ref:
            self._bvilive_state["last_ref"] = ref
            save_bvilive_state(self._bvilive_path, self._bvilive_state)

    def _browse_font(self):
        path = self._font_list_picker(self.font_var.get(), preview_attr="font_var")
        if path:
            self.font_var.set(path)

    def _browse_citefont(self):
        path = self._font_list_picker(
            self.citefont_var.get() or self.font_var.get(),
            preview_attr="citefont_var")
        if path:
            self.citefont_var.set(path)

    def _font_list_picker(self, current: str = "", preview_attr: str = "font_var") -> str:
        """Searchable font picker with All / Favorites views and live preview."""
        if sys.platform == "win32":
            win_fonts = os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts")
            dirs = [win_fonts]
        else:
            dirs = [
                str(Path.home() / "Library" / "Fonts"),
                "/Library/Fonts",
                "/System/Library/Fonts",
            ]
        exts = {".ttf", ".otf", ".ttc"}
        labels = {
            str(Path.home() / "Library" / "Fonts"): "Personal",
            "/Library/Fonts":                         "System",
            "/System/Library/Fonts":                  "Built-in",
        }
        entries: list = []
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

        preview_var_obj = getattr(self, preview_attr)
        original_font = preview_var_obj.get()
        favs: set = set(filter(None, self._bvilive_state.get("font_favorites", "").split("|")))
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
            if p and p != preview_var_obj.get():
                preview_var_obj.set(p)
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
            if current:
                cur_name = os.path.splitext(os.path.basename(current))[0]
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
            self._bvilive_state["font_favorites"] = "|".join(sorted(favs))
            save_bvilive_state(self._bvilive_path, self._bvilive_state)
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
            if preview_var.get() and preview_var_obj.get() != original_font:
                preview_var_obj.set(original_font)
                self.root.after(0, self._render)
            dlg.destroy()

        def confirm(*_):
            sel = lb.curselection()
            if sel:
                result.append(visible[sel[0]][1])
            # Restore the preview variable — the caller sets the right target var after we return
            if _preview_id[0]:
                self.root.after_cancel(_preview_id[0])
            if preview_var_obj.get() != original_font:
                preview_var_obj.set(original_font)
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

    def _browse_ref(self):
        """Three-column Book / Chapter / Verse picker dialog."""
        if not self.verse_index:
            return

        books = list(self.verse_index.keys())
        book0, ch0, vs0, _ = self._parse_ref()
        if book0 not in self.verse_index:
            book0 = books[0] if books else None
            ch0, vs0 = 1, 1
        if ch0 is None:
            ch0 = 1
        if vs0 is None:
            vs0 = 1

        verse_texts = self._get_verse_texts()

        dlg = tk.Toplevel(self.root)
        dlg.title("Go to Verse")
        dlg.grab_set()
        dlg.resizable(True, True)
        if self._browse_ref_geometry:
            dlg.geometry(self._browse_ref_geometry)
        result: list = []

        outer = ttk.Frame(dlg, padding=8)
        outer.pack(fill="both", expand=True)
        outer.columnconfigure(0, weight=0)
        outer.columnconfigure(1, weight=0)
        outer.columnconfigure(2, weight=1)
        outer.rowconfigure(1, weight=1)

        def _make_col(label_text, col, width, hscroll=False):
            ttk.Label(outer, text=label_text).grid(row=0, column=col, sticky="w",
                                                   padx=(0, 6), pady=(0, 2))
            frm = ttk.Frame(outer)
            frm.grid(row=1, column=col, sticky="nsew", padx=(0, 8 if col < 2 else 0))
            frm.columnconfigure(0, weight=1)
            frm.rowconfigure(0, weight=1)
            sb = ttk.Scrollbar(frm, orient="vertical")
            lb = tk.Listbox(frm, yscrollcommand=sb.set, width=width, height=30,
                            selectmode="single", activestyle="dotbox",
                            exportselection=False)
            sb.config(command=lb.yview)
            if hscroll:
                xsb = ttk.Scrollbar(frm, orient="horizontal")
                lb.config(xscrollcommand=xsb.set)
                xsb.config(command=lb.xview)
                xsb.pack(side="bottom", fill="x")
            sb.pack(side="right", fill="y")
            lb.pack(side="left", fill="both", expand=True)
            return lb

        book_lb  = _make_col("Book",    0, 20)
        ch_lb    = _make_col("Chapter", 1,  8)
        verse_lb = _make_col("Verse",   2, 56, hscroll=True)

        def _fill_chapters(book):
            ch_lb.delete(0, "end")
            if not book or book not in self.verse_index:
                return
            for ch in sorted(self.verse_index[book].keys()):
                ch_lb.insert("end", str(ch))

        def _fill_verses(book, ch):
            verse_lb.delete(0, "end")
            if not book or book not in self.verse_index:
                return
            max_vs = self.verse_index[book].get(ch, 0)
            for vs in range(1, max_vs + 1):
                ref  = f"{book} {ch}:{vs}"
                text = verse_texts.get(ref, "")
                verse_lb.insert("end", f"{vs:>3}  {text}")

        def _on_book_select(_=None):
            sel = book_lb.curselection()
            if not sel:
                return
            book = books[sel[0]]
            _fill_chapters(book)
            chapters = sorted(self.verse_index.get(book, {}).keys())
            if chapters:
                ch_lb.selection_set(0)
                ch_lb.see(0)
                _fill_verses(book, chapters[0])
                if verse_lb.size():
                    verse_lb.selection_set(0)
                    verse_lb.see(0)

        def _on_ch_select(_=None):
            bsel = book_lb.curselection()
            csel = ch_lb.curselection()
            if not bsel or not csel:
                return
            book = books[bsel[0]]
            ch   = int(ch_lb.get(csel[0]))
            _fill_verses(book, ch)
            if verse_lb.size():
                verse_lb.selection_set(0)
                verse_lb.see(0)

        def _current_ref() -> str:
            bsel = book_lb.curselection()
            csel = ch_lb.curselection()
            vsel = verse_lb.curselection()
            if not bsel or not csel or not vsel:
                return ""
            book  = books[bsel[0]]
            ch    = int(ch_lb.get(csel[0]))
            vs    = int(verse_lb.get(vsel[0]).split()[0])
            return f"{book} {ch}:{vs}"

        def _save_pos():
            self._browse_ref_geometry = dlg.geometry()

        def confirm(*_):
            _save_pos()
            ref = _current_ref()
            if ref:
                result.append(ref)
            dlg.destroy()

        def cancel(*_):
            _save_pos()
            dlg.destroy()

        book_lb.bind("<<ListboxSelect>>", _on_book_select)
        ch_lb.bind("<<ListboxSelect>>",   _on_ch_select)
        verse_lb.bind("<Double-1>",       confirm)
        verse_lb.bind("<Return>",         confirm)
        dlg.protocol("WM_DELETE_WINDOW",  cancel)

        # Bottom buttons
        bf = ttk.Frame(outer)
        bf.grid(row=2, column=0, columnspan=3, sticky="ew", pady=(8, 0))
        ttk.Button(bf, text="Cancel", command=cancel).pack(side="right", padx=(4, 0))
        ttk.Button(bf, text="OK",     command=confirm).pack(side="right")

        # Initial population
        for b in books:
            book_lb.insert("end", b)
        _fill_chapters(book0)
        _fill_verses(book0, ch0)

        # Pre-select current book
        if book0 in books:
            bi = books.index(book0)
            book_lb.selection_set(bi)
            book_lb.see(bi)

        # Pre-select current chapter
        chapters = sorted(self.verse_index.get(book0, {}).keys())
        if ch0 in chapters:
            ci = chapters.index(ch0)
            ch_lb.selection_set(ci)
            ch_lb.see(ci)

        # Pre-select current verse
        max_vs = self.verse_index.get(book0, {}).get(ch0, 0)
        if max_vs > 0:
            vi = max(0, min(vs0 - 1, max_vs - 1))
            verse_lb.selection_set(vi)
            verse_lb.see(vi)

        dlg.wait_window()
        if result:
            self.ref_var.set(result[0])
            self._schedule(0)

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

    def _build_preview(self):
        self.canvas = tk.Label(self.win, bg="black",
                               width=PREVIEW_W, height=PREVIEW_H)
        self.canvas.pack(fill="both", expand=True)

    def _position_windows(self):
        self.root.update_idletasks()
        self.win.update_idletasks()
        cw = self.root.winfo_reqwidth()
        self.root.geometry(f"+80+200")
        self.win.geometry(f"{PREVIEW_W}x{PREVIEW_H}+{80 + cw + 16}+200")
        self.root.lift()
        self.root.focus_force()

    # ── Index / navigation ────────────────────────────────────────────────────

    def _load_index(self, version: str):
        if version in self._index_cache:
            self.verse_index = self._index_cache[version]
            return
        filename = BIBLES.get(version, "BibleKJV.txt")
        index, verses = load_verse_index(self.bible_dir, filename)
        self.verse_index = index
        self._index_cache[version] = index
        self._verse_text_cache[version] = verses   # pre-warm; avoids second file read on first render

    def _on_version_change(self):
        self._load_index(self.version_var.get())
        self._schedule(0)

    def _parse_ref(self):
        """Return (book, ch, vs_start, vs_end); vs_end == vs_start for single verses."""
        m = re.match(r'^(.+?)\s+(\d+):(\d+)(?:-(\d+))?-?$', self.ref_var.get().strip())
        if not m:
            return None, None, None, None
        vs_start = int(m.group(3))
        vs_end   = int(m.group(4)) if m.group(4) else vs_start
        return m.group(1), int(m.group(2)), vs_start, vs_end

    def _step(self, delta: int):
        book, ch, vs_start, vs_end = self._parse_ref()
        if book is None:
            return
        vs = (vs_end if delta > 0 else vs_start) + delta
        ch_map = self.verse_index.get(book, {})
        max_vs = ch_map.get(ch, 0)
        if vs < 1:
            if ch > 1:
                ch -= 1
                vs = self.verse_index.get(book, {}).get(ch, 1)
            else:
                vs = 1
        elif max_vs and vs > max_vs:
            if ch_map.get(ch + 1):
                ch += 1
                vs = 1
            else:
                vs = max_vs
        self.ref_var.set(f"{book} {ch}:{vs}")
        self._schedule(0)

    def _step_chapter(self, delta: int):
        book, ch, vs_start, vs_end = self._parse_ref()
        if book is None:
            return
        ch += delta
        ch_map = self.verse_index.get(book, {})
        if ch < 1:
            ch = 1
        elif ch_map and ch > max(ch_map.keys()):
            ch = max(ch_map.keys())
        self.ref_var.set(f"{book} {ch}:1")
        self._schedule(0)

    # ── Pillow font-size fitting (fast path) ─────────────────────────────────
    # ImageMagick's caption: auto-fit can be slow (especially on Windows, ~8 s).
    # When a font file path is set, we replicate the fit in Python with Pillow
    # (< 10 ms) and pass --textsize=N so bvi renders at a fixed size instead.
    # We also inject --text= so bvi skips Bible loading entirely.
    # Falls back to normal ImageMagick auto-fit when no font file path is set.

    def _get_verse_texts(self) -> dict:
        version = self.version_var.get()
        if version not in self._verse_text_cache:
            filename = BIBLES.get(version, "BibleKJV.txt")
            path = _find_bible_path(self.bible_dir, filename)
            verses: dict = {}
            try:
                with open(path, encoding="utf-8-sig") as fh:
                    for line in fh:
                        line = line.rstrip("\n\r")
                        if "\t" not in line:
                            continue
                        ref, text = line.split("\t", 1)
                        verses[ref] = text
            except Exception:
                pass
            self._verse_text_cache[version] = verses
        return self._verse_text_cache[version]

    def _lookup_text(self) -> str:
        """Return bare verse or custom text (no curly-quote wrapping)."""
        if self.customtext_enabled.get():
            return self.customtext_var.get().strip()
        verses = self._get_verse_texts()
        ref = self.ref_var.get().strip()
        colon = ref.rfind(":")
        if colon == -1:
            return ""
        base = ref[:colon + 1]
        after = ref[colon + 1:]
        dash = after.find("-")
        if dash == -1:
            return verses.get(ref, "")
        try:
            start = int(after[:dash])
        except ValueError:
            return ""
        end_str = after[dash + 1:]
        if not end_str:
            end = max(
                (int(k[len(base):]) for k in verses
                 if k.startswith(base) and k[len(base):].isdigit()),
                default=start)
        else:
            try:
                end = int(end_str)
            except ValueError:
                return ""
        parts = [verses.get(f"{base}{v}", "") for v in range(start, end + 1)]
        return " ".join(p for p in parts if p)

    def _fit_fontsize_pillow(self, text: str, max_cap: int = 0,
                             img_w: int = 0, img_h: int = 0) -> int:
        """Return largest pointsize where text fits the verse area; 0 = unable."""
        font_path = self.font_var.get().strip()
        if sys.platform == "win32" and not os.path.isfile(font_path):
            # font_path may be empty or a PostScript name (valid on macOS, not a file on Windows).
            # Fall back to guaranteed Windows fonts so the fast Pillow path is always available.
            for _candidate in (r"C:\Windows\Fonts\georgia.ttf",
                               r"C:\Windows\Fonts\arial.ttf",
                               r"C:\Windows\Fonts\pala.ttf",
                               r"C:\Windows\Fonts\calibri.ttf",
                               r"C:\Windows\Fonts\times.ttf"):
                if os.path.isfile(_candidate):
                    font_path = _candidate
                    break
        if not font_path or not os.path.isfile(font_path):
            return 0
        if self.quotes_var.get() and not self.customtext_enabled.get():
            text = "“" + text + "”"
        try:
            if not img_w:
                img_w = int(self.width_var.get().strip() or "1920")
            if not img_h:
                img_h = int(self.height_var.get().strip() or "1080")
        except ValueError:
            return 0
        ts = self.textscale_var.get().strip()
        tscale = int(ts) if re.fullmatch(r"\d+", ts) else 100
        target_w = int(img_w * 0.896 * tscale / 100.0)
        target_h = int(img_h * 0.741 * tscale / 100.0)
        for _side, _var in (("top", self.reserve_top_var), ("right", self.reserve_right_var),
                            ("bottom", self.reserve_bottom_var), ("left", self.reserve_left_var)):
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
        try:
            with open(font_path, "rb") as _fh:
                _font_bytes = _fh.read()
        except OSError:
            return 0
        lo, best, words = 8, 8, text.split()
        while lo <= hi:
            mid = (lo + hi) // 2
            try:
                font = ImageFont.truetype(io.BytesIO(_font_bytes), mid)
            except Exception:
                return 0
            lines, cur = [], []
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
            max_w = max(font.getlength(ln) for ln in lines)
            if max_w <= target_w and total_h <= target_h:
                best, lo = mid, mid + 1
            else:
                hi = mid - 1
        return best

    # ── Rendering ─────────────────────────────────────────────────────────────

    def _toggle_live(self):
        self._live_active = not self._live_active
        self.live_btn.config(text="Pause" if self._live_active else "Go Live")
        if self._live_active:
            self._schedule(0)

    def _schedule(self, delay_ms: int = 400):
        if not self._live_active:
            return
        if self._after_id is not None:
            self.root.after_cancel(self._after_id)
        self._after_id = self.root.after(delay_ms, self._render)

    def _build_cmd(self) -> list:
        ref = self.ref_var.get().strip()
        text_mode = self.customtext_enabled.get()
        effective_citestyle = "none" if text_mode else self.citestyle_var.get()
        cmd = [BVI, ref,
               f"--bibleversion={self.version_var.get()}",
               f"--output={TMP_JPG}",
               f"--citestyle={effective_citestyle}"]
        if text_mode:
            ct = self.customtext_var.get()
            if ct:
                cmd.append(f"--text={ct}")
        # Render at half the configured resolution for live preview — visually
        # equivalent but ~4× fewer pixels for ImageMagick to process.
        try:
            _cw = int(self.width_var.get().strip() or "1920")
            _ch = int(self.height_var.get().strip() or "1080")
        except ValueError:
            _cw, _ch = 1920, 1080
        _pw = max(320, _cw // 2)
        _ph = max(180, _ch // 2)

        mf     = self.maxtextsize_var.get().strip()
        tp_abs = self.textsize_var.get().strip()
        ts     = self.textscale_var.get().strip()
        has_fixed = bool(re.fullmatch(r'\d+', tp_abs) and int(tp_abs) > 0)
        fitted = 0
        if not has_fixed:
            max_cap = int(mf) if re.fullmatch(r'\d+', mf) and int(mf) > 0 else 0
            lookup = self._lookup_text()
            if lookup:
                fitted = self._fit_fontsize_pillow(lookup, max_cap,
                                                   img_w=_pw, img_h=_ph)
                if fitted > 0 and not text_mode:
                    cmd.append(f"--text={lookup}")
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
        citefont = self.citefont_var.get().strip()
        if citefont:
            cmd.append(f"--citefont={citefont}")
        cs = self.citescale_var.get().strip()
        cmd.append(f"--citescale={cs}" if re.fullmatch(r'\d+', cs) else "--citescale=100")
        cmd.append(f"--width={_pw}")
        cmd.append(f"--height={_ph}")
        if self.quotes_var.get() and not text_mode:
            cmd.append("--quotes")
        bg = self.bg_var.get().strip()
        if bg:
            cmd.append(f"--bg={bg}")
        tc = self.textcolor_var.get().strip()
        if tc:
            cmd.append(f"--textcolor={tc}")
        cc = self.citecolor_var.get().strip()
        if cc:
            cmd.append(f"--citecolor={cc}")
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
        for _side, _var in (("top", self.reserve_top_var), ("right", self.reserve_right_var),
                            ("bottom", self.reserve_bottom_var), ("left", self.reserve_left_var)):
            _pct = _var.get().strip()
            if re.fullmatch(r'\d+', _pct) and int(_pct) > 0:
                cmd.append(f"--reserve={_side},{_pct}")
        cmd.append(f"--citebibleversion={'yes' if self.citebibleversion_var.get() else 'no'}")
        cmd.append(f"--citeplacement={self.citeplacement_var.get()}")
        cmd.append(f"--citealign={self.citealign_var.get()}")
        cmd.append(f"--citepanel={self.citepanel_var.get()}")
        toffy = self.textoffy_var.get().strip()
        if re.fullmatch(r'-?\d+', toffy) and toffy != "0":
            cmd.append(f"--textoffy={toffy}")
        coffy = self.citeoffy_var.get().strip()
        if re.fullmatch(r'-?\d+', coffy) and coffy != "0":
            cmd.append(f"--citeoffy={coffy}")
        cs = self.citeshadow_var.get().strip()
        if re.fullmatch(r'\d+', cs) and int(cs) > 0:
            cmd.append(f"--citeshadow={cs}")
        else:
            cmd.append("--no-citeshadow")
        return cmd

    def _on_key_left(self, event):
        if isinstance(event.widget, tk.Entry):
            return   # let the entry handle cursor movement
        self._step(-1)

    def _on_key_right(self, event):
        if isinstance(event.widget, tk.Entry):
            return
        self._step(+1)

    def _render(self):
        self._after_id = None
        ref = self.ref_var.get().strip()
        if not ref:
            return
        proc = self._bvi_proc
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
        threading.Thread(target=self._run_bvi, args=(cmd, gen), daemon=True).start()

    def _run_bvi(self, cmd: list, gen: int):
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self._bvi_proc = proc
        stdout, stderr = proc.communicate()
        self._bvi_proc = None
        result = subprocess.CompletedProcess(cmd, proc.returncode,
                                             stdout.decode(errors="replace"),
                                             stderr.decode(errors="replace"))
        self.root.after(0, self._render_done, result, gen)

    def _render_done(self, result, gen: int):
        if gen != self._render_gen:
            return  # a newer render superseded this one; discard
        if result.returncode != 0:
            lines = (result.stderr or result.stdout).strip().splitlines()
            msg = lines[0] if lines else "bvi error"
            self.status_var.set(msg)
            return
        try:
            img = Image.open(TMP_JPG)
            self._display_image(img)
            ref = self.ref_var.get().strip()
            self.status_var.set(f"{ref}  ({self.version_var.get()})")
            self._save_last_ref(ref)
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
    BviView().run()
