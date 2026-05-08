# textimage

A C++ program that renders plain text to a JPEG image with auto-fitted text. The text is centered on the canvas. An optional second text can be provided. There are many features to use on the text for sizing and how it presents. 

## Features

- Auto-sizes font to fill the image — longer text uses a smaller font, short text uses a larger one
- Text centered horizontally and vertically on the canvas
- Hard line breaks via `\n` in the text argument
- Configurable image size (default 1920×1080)
- Configurable colors for background and text
- Photo background support with adjustable dimming for text readability
- Text size: force an absolute point size (`--textsize`), cap auto-fit at a maximum (`--maxtextsize`), or scale relative to auto-fit (`--textscale`)
- Semi-transparent panel behind text for readability (`--textpanel`)
- Drop shadow behind text with adjustable intensity (`--textshadow[=N]`)
- Outline around text for stronger emphasis (`--textoutline[=N]`, `--textoutlinecolor`)
- Shadow style: soft Gaussian blur or hard offset copy (`--shadowmethod`)
- Adjustable line spacing (`--linespacing`)
- Reserve a portion of the image from any side, centering text in the remaining space (`--reserve`)
- Custom font support
- Config file (`.verselumen` `[textimage]` section) stores per-folder defaults
- Default output filename derived from the first few words of the text

## Building

**macOS / Linux (g++):**
```bash
g++ -std=c++17 -o textimage textimage.cpp
```

**Windows (MSVC — run in Developer Command Prompt):**
```bat
cl /EHsc /std:c++17 textimage.cpp /Fe:textimage.exe
```

## Requirements

- C++11 or later
- ImageMagick 7: `brew install imagemagick`

## Usage

```bash
./textimage "He is risen."
./textimage "Be still and know that I am God."
./textimage --text="Grace upon grace."
```

Specify an output file:
```bash
./textimage "He is risen." --output=risen.jpg
```

Change image dimensions:
```bash
./textimage "He is risen." --width=1280 --height=720
./textimage "He is risen." --width=1080 --height=1080
```

## Line Breaks

Use `\n` in the text to force a line break at that point. ImageMagick's `caption:` operator treats embedded newlines as hard line breaks, overriding its automatic word-wrap.

```bash
./textimage "He is risen.\nDeath has no power."
./textimage "Line one\nLine two\nLine three"
./textimage "Verse one\n\nVerse two"   # double \n = blank line between stanzas
```

## Background

### Solid color

Colors accept any value ImageMagick understands: named colors, hex codes, or ImageMagick built-ins.

```bash
./textimage "He is risen." --bg=black --textcolor=white
./textimage "He is risen." --bg="#1a1a2e" --textcolor="#e8e8f0"
./textimage "He is risen." --bg=navy --textcolor=gold
```

| Option | Default | Description |
|---|---|---|
| `--bg=COLOR` | `black` | Background color |
| `--textcolor=COLOR` | `white` | Text color |

### Photo background

Use any JPEG or PNG as the background. The photo is resized and cropped to fill the canvas. A darkening pass (`--dim`) improves text readability over busy photos.

```bash
./textimage "He is risen." --bgphoto="sunset.jpg"
./textimage "He is risen." --bgphoto="photo.jpg" --dim=40
./textimage "He is risen." --bgphoto="photo.png" --dim=0 --textcolor=white
```

| Option | Default | Description |
|---|---|---|
| `--bgphoto=FILE` | _(none)_ | Background photo; overrides `--bg` |
| `--dim=N` | `50` | Darken photo 0–100% (0 = no dim, 100 = black) |

## Text Readability

### Panel

A semi-transparent colored rectangle drawn behind the text block improves contrast against photo backgrounds and on projector screens.

```bash
./textimage "He is risen." --bgphoto=photo.jpg --textpanel=60
./textimage "He is risen." --bgphoto=photo.jpg --textpanel=70 --textpanelcolor="#001020"
./textimage "He is risen." --textpanel=50 --textpanelrounded
```

| Option | Default | Description |
|---|---|---|
| `--textpanel=N` | `0` (off) | Panel opacity 1–100 |
| `--textpanelcolor=COLOR` | `black` | Panel color; any ImageMagick color |
| `--textpanelrounded` | off | Rounded corners on the panel |
| `--no-textpanelrounded` | _(default)_ | Square corners |

### Shadow

A drop shadow behind the text adds depth and separation from the background.

```bash
./textimage "He is risen." --bgphoto=photo.jpg --textshadow       # intensity 5 (default)
./textimage "He is risen." --textshadow=3                         # lighter shadow
./textimage "He is risen." --textshadow=8                         # heavier shadow
./textimage "He is risen." --no-textshadow                        # explicitly off
```

The `N` value ranges from 1 (lightest) to 10 (heaviest). The bare `--textshadow` flag uses intensity 5.

Panel and shadow can be combined:

```bash
./textimage "He is risen." --bgphoto=photo.jpg --textpanel=50 --textshadow
```

### Shadow Method

Control the rendering style with `--shadowmethod`:

| Value | Style | Description |
|---|---|---|
| `1` (default) | Soft / Gaussian | Black shadow blurred with a Gaussian filter at 80% opacity |
| `2` | Hard / copy | Solid black offset copy of the text, no blur |

```bash
./textimage "He is risen." --textshadow=6 --shadowmethod=1   # soft (default)
./textimage "He is risen." --textshadow=6 --shadowmethod=2   # hard
```

### Text Outline

Add a solid outline around text characters for stronger emphasis than a drop shadow. The outline is rendered cleanly outside the glyphs using morphological dilation, so the fill color is never obscured.

```bash
./textimage "He is risen." --textoutline            # 2px black outline (default width/color)
./textimage "He is risen." --textoutline=3          # 3px outline
./textimage "He is risen." --textoutline=4 --textoutlinecolor=navy
./textimage "He is risen." --no-textoutline         # off (default)
```

| Option | Description |
|---|---|
| `--textoutline[=N]` | Outline width in pixels (default 2 when flag used without value) |
| `--no-textoutline` | Disable outline (default) |
| `--textoutlinecolor=C` | Outline color, any ImageMagick color (default: black) |

When combined with `--textshadow`, the outline is applied first so the shadow is cast from the outlined shape, which looks natural. Outline width of 2–4px suits most large text; higher values produce bolder borders.

### Line Spacing

Adjust the amount of space between lines of text with `--linespacing`:

```bash
./textimage "He is risen." --linespacing=10    # more space between lines
./textimage "He is risen." --linespacing=-5    # tighter lines
./textimage "He is risen." --linespacing=0     # ImageMagick default (no adjustment)
```

Positive values increase the gap; negative values reduce it. The default is `0` (no adjustment).

## Position Offset

Fine-tune the vertical position of the text block with `--textoffy`. Positive moves it down, negative moves it up.

```bash
./textimage "He is risen." --textoffy=30    # move text 30px down
./textimage "He is risen." --textoffy=-20   # move text 20px up
```

Defaults to `0` (no adjustment).

## Reserve

Keep a portion of the image empty on one or more sides and center the text in the remaining space. Useful for placing text alongside a photo subject, a logo, or another UI element.

Use `--reserve=SIDE,PCT` once per side. Each side independently shrinks the text canvas and shifts the center into the remaining region. Multiple sides compose correctly.

```bash
./textimage "He is risen." --reserve=top,30                          # reserve 30% at top; text in bottom 70%
./textimage "He is risen." --reserve=right,40                        # reserve 40% at right; text in left 60%
./textimage "He is risen." --reserve=left,30 --reserve=top,20        # reserve left 30% and top 20%; text in remaining bottom-right area
./textimage "He is risen." --reserve=left,25 --reserve=right,25      # center column; equal margins
```

SIDE is `top`, `right`, `bottom`, or `left`; PCT is 0–90.

| Option | Description |
|---|---|
| `--reserve=SIDE,PCT` | Reserve PCT% from SIDE; repeat for multiple sides |

Reserve can be combined with `--textoffy` for fine-tuning within the remaining area:

```bash
./textimage "He is risen." --reserve=top,30 --reserve=left,20 --textoffy=10
```

## Stacked Text

Add a second text block below the first with `--text2`. Both blocks auto-fit to half the available height so the combined result stays centered on the canvas.

```bash
./textimage "He is risen." --text2="Death has no power."
./textimage "He is risen." --text2="Death has no power." --text2gap=100
```

Control the gap between the two blocks with `--text2gap=N` (pixels, default 40):

| Option | Default | Description |
|---|---|---|
| `--text2=TEXT` | _(none)_ | Second text block stacked below the first |
| `--text2gap=N` | `40` | Pixel gap between the two text blocks |
| `--text2color=COLOR` | _(same as `--textcolor`)_ | Text color for the second block |
| `--text2font=FONT` | _(same as `--font`)_ | Font for the second block |

Options like `--textpanel`, `--bgphoto`, and `--dim` apply to the combined image. Shadow and outline can be set independently for each block (see below).

`\n` works in `--text2` the same as in the primary text:

```bash
./textimage "He is risen." --text2="Hallelujah.\nAmen."
```

### Per-block shadow and outline

By default `--text2` inherits `--textshadow`, `--shadowmethod`, `--textoutline`, and `--textoutlinecolor` from the primary text block. Override any of these independently for the second block:

```bash
./textimage "He is risen." --textshadow=5 --text2="Death has no power." --text2shadow=0
./textimage "He is risen." --textoutline=2 --text2="Hallelujah." --text2outline=4 --text2outlinecolor=gold
./textimage "He is risen." --text2="Amen." --text2shadowmethod=2
```

| Option | Default | Description |
|---|---|---|
| `--text2outline[=N]` | _(same as `--textoutline`)_ | Outline width in pixels for the second block |
| `--no-text2outline` | — | Disable outline for the second block |
| `--text2outlinecolor=C` | _(same as `--textoutlinecolor`)_ | Outline color for the second block |
| `--text2shadow[=N]` | _(same as `--textshadow`)_ | Shadow intensity 1–10 for the second block |
| `--no-text2shadow` | — | Disable shadow for the second block |
| `--text2shadowmethod=N` | _(same as `--shadowmethod`)_ | Shadow style for the second block (`1` = soft, `2` = hard) |

When a text2 override is not specified the option inherits from the corresponding text1 setting, so setting `--textshadow=5` applies to both blocks unless `--text2shadow` overrides it.

## Text Size

By default the text auto-fits to the largest size that fills the canvas. Use one of these options to control the size — `--textsize`/`--maxtextsize` and `--textscale` cannot be combined.

**`--textscale=PCT`** — shrink the text area to a percentage of its default size. The font still auto-fits within the smaller area, so the result is proportionally smaller text:

```bash
./textimage "He is risen." --textscale=75    # ~75% of normal size
./textimage "He is risen." --textscale=50    # ~half size
```

**`--maxtextsize=N`** — cap the auto-fit font at N points. The font fills the canvas up to that maximum and won't exceed it:

```bash
./textimage "He is risen." --maxtextsize=140
./textimage "He is risen." --maxtextsize=80
```

**`--textsize=N`** — force the font to exactly N points, bypassing auto-fit entirely. Very long text may overflow the canvas at large sizes:

```bash
./textimage "He is risen." --textsize=160
./textimage "He is risen." --textsize=72
```

## Font

The default font is `Palatino` (macOS), `Palatino Linotype` (Windows), or `DejaVu-Serif` (Linux). Override with a font name or file path:

```bash
./textimage "He is risen." --font="Georgia"
./textimage "He is risen." --font="/Library/Fonts/MyFont.ttf"
```

To list fonts available to ImageMagick:
```bash
magick -list font
```

## Config File (.verselumen [textimage])

Run `--saveconfig` to save the current settings as defaults for the current folder. Any subsequent `textimage` run in that folder will use those values automatically. Command-line arguments always override the config.

Save current settings as defaults:
```bash
./textimage --bg="#1a1a2e" --textcolor="#e8e8f0" --textscale=80 --saveconfig
```

Show current effective settings:
```bash
./textimage --showconfig
```

The `[textimage]` section of `.verselumen` is plain text and can be edited by hand:
```
[textimage]
width            = 1920
height           = 1080
font             = /System/Library/Fonts/Palatino.ttc
bg               = #1a1a2e
bgphoto          =
dim              = 50
textcolor        = #e8e8f0
textsize         = 0
maxtextsize      = 0
textscale        = 100
textpanel        = 0
textpanelcolor   = black
textpanelrounded = no
textshadow       = no
shadowmethod     = 1
textoutline      = 0
textoutlinecolor = black
linespacing      = 0
textoffy         = 0
reservetop       = 20
reserveright     = 0
reservebottom    = 0
reserveleft      = 30
text2outline     =
text2outlinecolor=
text2shadow      =
text2shadowmethod=
```

Supported keys: `width`, `height`, `font`, `bg`, `bgphoto`, `dim`, `textcolor`, `textsize`, `maxtextsize`, `textscale`, `textpanel`, `textpanelcolor`, `textpanelrounded`, `textshadow`, `shadowmethod`, `textoutline`, `textoutlinecolor`, `linespacing`, `textoffy`, `reservetop`, `reserveright`, `reservebottom`, `reserveleft`, `text2outline`, `text2outlinecolor`, `text2shadow`, `text2shadowmethod`

The `text2` override keys are blank by default (inherit from the corresponding text1 setting). Set them to a value only when you want to override.

## Files

- `textimage.cpp` — Source code
- `.verselumen` — Shared config file; `[textimage]` section created by `--saveconfig`
