# bvi

A C++ program that renders a Bible verse reference (or custom text) to a JPEG image with auto-fitted text. The verse is centered on the canvas with an optional citation line.

## Features

- Auto-sizes font to fill the image — longer passages use a smaller font, short verses use a larger one
- Text centered horizontally and vertically on the canvas
- Citation line with configurable style, placement, and size
- Support for multiple Bible translations:
  - **KJV** (King James Version) — default
  - **BSB** (Berean Standard Bible)
  - **WEB** (World English Bible)
- Verse ranges (`John 3:16-17`), verse-to-end-of-chapter (`Romans 8:28-`)
- Custom text mode (`--text`) bypasses Bible lookup entirely
- Configurable image size (default 1920×1080)
- Configurable colors for background, verse text, and citation
- Photo background support with adjustable dimming for text readability
- Optional curly quotation marks around the verse text
- Citation style: em-dash prefix, parentheses-wrapped, plain, or omitted (`--citestyle`)
- Citation placement: fixed at bottom edge or attached just below verse text (`--citeplacement`)
- Citation font size: absolute (`--citesize`) or relative to auto (`--citescale`)
- Citation font independent from verse font (`--citefont`)
- Citation drop shadow with adjustable intensity (`--citeshadow[=N]`)
- Option to omit the Bible version from the citation (`--citebibleversion=no`)
- Verse text size: force an absolute point size (`--textsize`), cap auto-fit at a maximum (`--maxtextsize`), or scale relative to auto-fit (`--textscale`)
- Semi-transparent panel behind verse text for readability (`--textpanel`)
- Drop shadow behind verse text with adjustable intensity (`--textshadow[=N]`)
- Outline around verse text for stronger emphasis (`--textoutline[=N]`, `--textoutlinecolor`)
- Shadow style: soft Gaussian blur or hard offset copy (`--shadowmethod`)
- Adjustable line spacing (`--linespacing`)
- Reserve a portion of the image from any side, centering verse text in the remaining space (`--reserve`)
- Custom font support
- Config file (`.verselumen` `[bvi]` section) stores per-folder defaults for version, size, colors, and style
- Default output filename derived from the reference (e.g. `John_3_16.jpg`)
- Auto-prompts to download the Bible translation file if not found

## Building

**macOS / Linux (g++):**
```bash
g++ -std=c++11 -o bvi bvi.cpp
```

**Windows (MSVC — run in Developer Command Prompt):**
```bat
cl /EHsc /std:c++17 bvi.cpp /Fe:bvi.exe
```

## Requirements

- C++11 or later
- ImageMagick 7: `brew install imagemagick`
- `curl` (for automatic Bible file download)

## Usage

```bash
./bvi "John 3:16"
./bvi "Philippians 4:6-7"
./bvi "Romans 8:28-"
```

The reference can also be passed with a named flag (consistent with `bv`):
```bash
./bvi --ref="John 3:16"
./bvi -ref="Philippians 4:6-7"
```

Specify a Bible version:
```bash
./bvi "John 3:16" -bv=BSB
./bvi "John 3:16" --bibleversion=WEB
```

Specify an output file:
```bash
./bvi "John 3:16" --output=john316.jpg
```

Change image dimensions:
```bash
./bvi "John 3:16" --width=1280 --height=720
./bvi "John 3:16" --width=1080 --height=1080
```

## Custom Text

Use `--text` to render arbitrary text instead of a Bible verse lookup. No Bible file is required.

```bash
./bvi --text="He is risen." --bg=black --textcolor=white
./bvi --text="Peace be with you." --bgphoto=photo.jpg
```

The citation is suppressed automatically when `--text` is used without a reference. To show a citation alongside custom text, provide `-ref=`:

```bash
./bvi --text="He is risen." --ref="Luke 24:6"
```

The output filename defaults to `bvi_output.jpg` when no reference is given.

## Background

### Solid color

Colors accept any value ImageMagick understands: named colors, hex codes, or ImageMagick built-ins.

```bash
./bvi "John 3:16" --bg=black --textcolor=white --citecolor=gray60
./bvi "John 3:16" --bg="#1a1a2e" --textcolor="#e8e8f0" --citecolor="#8888bb"
./bvi "John 3:16" --bg=navy --textcolor=gold --citecolor=lightyellow
```

| Option | Default | Description |
|---|---|---|
| `--bg=COLOR` | `black` | Background color |
| `--textcolor=COLOR` | `white` | Verse text color |
| `--citecolor=COLOR` | `gray60` | Citation line color |

### Photo background

Use any JPEG or PNG as the background. The photo is resized and cropped to fill the canvas. A darkening pass (`--dim`) improves text readability over busy photos.

```bash
./bvi "John 3:16" --bgphoto="sunset.jpg"
./bvi "John 3:16" --bgphoto="photo.jpg" --dim=40
./bvi "John 3:16" --bgphoto="photo.png" --dim=0 --textcolor=white --citecolor=gray80
```

| Option | Default | Description |
|---|---|---|
| `--bgphoto=FILE` | _(none)_ | Background photo; overrides `--bg` |
| `--dim=N` | `50` | Darken photo 0–100% (0 = no dim, 100 = black) |

## Quotation Marks

Add curly `"` `"` quotation marks around the verse text:

```bash
./bvi "John 3:16" --quotes
./bvi "John 3:16" --no-quotes   # explicitly off (default)
```

## Text Readability

### Panel

A semi-transparent colored rectangle drawn behind the verse text block improves contrast against photo backgrounds and on projector screens.

```bash
./bvi "John 3:16" --bgphoto=photo.jpg --textpanel=60
./bvi "John 3:16" --bgphoto=photo.jpg --textpanel=70 --textpanelcolor="#001020"
```

| Option | Default | Description |
|---|---|---|
| `--textpanel=N` | `0` (off) | Panel opacity 1–100 |
| `--textpanelcolor=COLOR` | `black` | Panel color; any ImageMagick color |

**Panel and citation placement:**

- `--citeplacement=below` — the panel automatically extends downward to enclose the citation as well as the verse text.
- `--citeplacement=bottom` (default) — behavior depends on `--citepanel`:

| `--citepanel` value | Effect |
|---|---|
| `independent` (default) | Main panel covers verse text; a separate narrow panel of the same color and opacity is drawn behind the citation at the bottom |
| `coverbottom` | Main panel stretches downward from the verse text all the way to cover the citation at the bottom — one continuous panel |
| `none` | Main panel covers only the verse text; no panel behind the citation |

```bash
./bvi "John 3:16" --bgphoto=photo.jpg --textpanel=60 --citepanel=coverbottom
./bvi "John 3:16" --bgphoto=photo.jpg --textpanel=60 --citepanel=none
./bvi "John 3:16" --bgphoto=photo.jpg --textpanel=60 --citepanel=independent  # default
```

### Shadow

A drop shadow behind the verse text adds depth and separation from the background.

```bash
./bvi "John 3:16" --bgphoto=photo.jpg --textshadow       # intensity 5 (default)
./bvi "John 3:16" --textshadow=3                         # lighter shadow
./bvi "John 3:16" --textshadow=8                         # heavier shadow
./bvi "John 3:16" --no-textshadow                        # explicitly off
```

The `N` value ranges from 1 (lightest) to 10 (heaviest). The bare `--textshadow` flag uses intensity 5.

Panel and shadow can be combined:

```bash
./bvi "John 3:16" --bgphoto=photo.jpg --textpanel=50 --textshadow
```

### Shadow Method

Control the rendering style for both text and citation shadows with `--shadowmethod`:

| Value | Style | Description |
|---|---|---|
| `1` (default) | Soft / Gaussian | Black shadow blurred with a Gaussian filter at 80% opacity |
| `2` | Hard / copy | Solid black offset copy of the text, no blur |

```bash
./bvi "John 3:16" --textshadow=6 --shadowmethod=1   # soft (default)
./bvi "John 3:16" --textshadow=6 --shadowmethod=2   # hard
```

The shadow method affects both `--textshadow` and `--citeshadow` together.

### Text Outline

Add a solid outline around verse text characters for stronger emphasis than a drop shadow. The outline is rendered cleanly outside the glyphs using morphological dilation, so the fill color is never obscured.

```bash
./bvi "John 3:16" --textoutline            # 2px black outline (default width/color)
./bvi "John 3:16" --textoutline=3          # 3px outline
./bvi "John 3:16" --textoutline=4 --textoutlinecolor=navy
./bvi "John 3:16" --no-textoutline         # off (default)
```

| Option | Description |
|---|---|
| `--textoutline[=N]` | Outline width in pixels (default 2 when flag used without value) |
| `--no-textoutline` | Disable outline (default) |
| `--textoutlinecolor=C` | Outline color, any ImageMagick color (default: black) |

When combined with `--textshadow`, the outline is applied first so the shadow is cast from the outlined shape, which looks natural. Outline width of 2–4px suits most large text; higher values produce bolder borders.

### Line Spacing

Adjust the amount of space between lines of verse text with `--linespacing`:

```bash
./bvi "John 3:16" --linespacing=10    # more space between lines
./bvi "John 3:16" --linespacing=-5    # tighter lines
./bvi "John 3:16" --linespacing=0     # ImageMagick default (no adjustment)
```

Positive values increase the gap; negative values reduce it. The default is `0` (no adjustment). This passes `-interline-spacing N` to ImageMagick's `caption:` operator.

## Position Offsets

Fine-tune the vertical position of the verse text and citation independently.

**`--textoffy=N`** — shift the verse text block vertically. Positive moves it down, negative moves it up.

```bash
./bvi "John 3:16" --textoffy=30    # move verse text 30px down
./bvi "John 3:16" --textoffy=-20   # move verse text 20px up
```

**`--citeoffy=N`** — shift the citation vertically. For `bottom` placement, positive moves it closer to the bottom edge; for `below` placement, positive moves it further from the verse block.

```bash
./bvi "John 3:16" --citeoffy=20    # push citation closer to bottom
./bvi "John 3:16" --citeoffy=-10   # pull citation away from bottom
./bvi "John 3:16" --citeplacement=below --citeoffy=10   # add extra gap below verse
```

Both default to `0` (no adjustment).

## Reserve

Keep a portion of the image empty on one or more sides and center the verse text in the remaining space. Useful for placing text alongside a photo subject, a logo, or another UI element.

Use `--reserve=SIDE,PCT` once per side. Each side independently shrinks the text canvas and shifts the center into the remaining region. Multiple sides compose correctly.

```bash
./bvi "John 3:16" --reserve=top,30                          # reserve 30% at top; verse in bottom 70%
./bvi "John 3:16" --reserve=right,40                        # reserve 40% at right; verse in left 60%
./bvi "John 3:16" --reserve=left,30 --reserve=top,20        # reserve left 30% and top 20%; verse in remaining bottom-right area
./bvi "John 3:16" --reserve=left,25 --reserve=right,25      # center column; equal margins
```

SIDE is `top`, `right`, `bottom`, or `left`; PCT is 0–90.

| Option | Description |
|---|---|
| `--reserve=SIDE,PCT` | Reserve PCT% from SIDE; repeat for multiple sides |

Reserve can be combined with `--textoffy` for fine-tuning within the remaining area:

```bash
./bvi "John 3:16" --reserve=top,30 --reserve=left,20 --textoffy=10
```

## Citation

### Style

Control the format of the citation line with `--citestyle`:

| Value | Output |
|---|---|
| `dash` (default) | `— Philippians 4:6-7 (KJV)` |
| `parens` | `(Philippians 4:6-7, KJV)` |
| `plain` | `Philippians 4:6-7 (KJV)` |
| `none` | _(no citation)_ |

```bash
./bvi "John 3:16" --citestyle=parens
./bvi "John 3:16" --citestyle=plain
./bvi "John 3:16" --citestyle=none
```

Use `--citebibleversion=no` (or `false`) to omit the Bible version entirely:

| With version (default) | Without version |
|---|---|
| `— John 3:16 (KJV)` | `— John 3:16` |
| `(John 3:16, KJV)` | `(John 3:16)` |
| `John 3:16 (KJV)` | `John 3:16` |

```bash
./bvi "John 3:16" --citebibleversion=no
./bvi "John 3:16" --citestyle=plain --citebibleversion=no
```

### Placement

Control where the citation appears with `--citeplacement`:

- `bottom` (default) — fixed near the bottom edge of the image
- `below` — positioned just underneath the verse text block

```bash
./bvi "John 3:16" --citeplacement=below
./bvi "John 3:16" --citeplacement=below --citestyle=plain
```

### Font

By default the citation uses the same font as the verse text. Override it independently with `--citefont`:

```bash
./bvi "John 3:16" --citefont="Georgia"
./bvi "John 3:16" --font="Palatino" --citefont="Helvetica"
./bvi "John 3:16" --citefont="/Library/Fonts/MyFont.ttf"
```

### Shadow

Add a drop shadow behind the citation text:

```bash
./bvi "John 3:16" --citeshadow                        # intensity 5 (default)
./bvi "John 3:16" --citeshadow=3                      # lighter
./bvi "John 3:16" --citeshadow=8                      # heavier
./bvi "John 3:16" --no-citeshadow                     # explicitly off
./bvi "John 3:16" --citeshadow --textshadow           # shadow on both verse and citation
```

The `N` value ranges from 1 (lightest) to 10 (heaviest). Shadow style is controlled by `--shadowmethod` (see above).

### Alignment

Control the horizontal alignment of the citation text with `--citealign`:

| Value | Description |
|---|---|
| `center` (default) | Citation centered horizontally |
| `left` | Citation left-aligned, inset to match the verse text margin |
| `right` | Citation right-aligned, inset to match the verse text margin |

```bash
./bvi "John 3:16" --citealign=left
./bvi "John 3:16" --citealign=right
./bvi "John 3:16" --citealign=center   # default
```

### Size

By default the citation point size is auto-scaled based on image height (~60pt at 1080p). Use one of these options to change it — they cannot be combined.

**`--citescale=PCT`** — scale the auto-calculated size by a percentage:

```bash
./bvi "John 3:16" --citescale=75    # ~75% of auto size
./bvi "John 3:16" --citescale=150   # larger than default
```

**`--citesize=N`** — set an absolute point size:

```bash
./bvi "John 3:16" --citesize=40
./bvi "John 3:16" --width=1080 --height=1080 --citesize=24
```

## Verse Text Size

By default the verse text auto-fits to the largest size that fills the canvas. Use one of these options to control the size — `--textsize`/`--maxtextsize` and `--textscale` cannot be combined.

**`--textscale=PCT`** — shrink the text area to a percentage of its default size. The font still auto-fits within the smaller area, so the result is proportionally smaller text:

```bash
./bvi "John 3:16" --textscale=75    # ~75% of normal size
./bvi "John 3:16" --textscale=50    # ~half size
```

**`--maxtextsize=N`** — cap the auto-fit verse font at N points. The font fills the canvas up to that maximum and won't exceed it. If both `--textsize` and `--maxtextsize` are provided, `--maxtextsize` takes priority.

At 1920×1080, typical auto-fit sizes by verse length: short verses (~50 chars) reach ~250pt; a median Luke verse (~111 chars) auto-fits to ~140pt; long passages (~200+ chars) drop to ~80pt. So **140pt** is a reasonable mid-range cap for typical single-verse use.

```bash
./bvi "Philippians 4:6-7" --maxtextsize=100
./bvi "John 3:16" --maxtextsize=140
```

**`--textsize=N`** — force the verse font to exactly N points, bypassing auto-fit entirely. The text renders at N pt; very long passages may overflow the canvas.

```bash
./bvi "John 3:16" --textsize=72
./bvi --text="He is risen." --textsize=160
```

## Font

The default font is `Palatino` (macOS), `Palatino Linotype` (Windows), or `DejaVu-Serif` (Linux). Override with a font name or file path:

```bash
./bvi "John 3:16" --font="Georgia"
./bvi "John 3:16" --font="/Library/Fonts/MyFont.ttf"
```

To list fonts available to ImageMagick:
```bash
magick -list font
```

## Config File (.verselumen [bvi])

Run `--saveconfig` to save the current settings as defaults for the current folder. Any subsequent `bvi` run in that folder will use those values automatically. Command-line arguments always override the config.

Save current settings as defaults:
```bash
./bvi --bg="#1a1a2e" --textcolor="#e8e8f0" --citecolor="#8888bb" \
      --citestyle=plain --citeplacement=below --textscale=80 --saveconfig
```

Show current effective settings:
```bash
./bvi --showconfig
```

The `[bvi]` section of `.verselumen` is plain text and can be edited by hand:
```
[bvi]
bv               = KJV
width            = 1920
height           = 1080
font             = /System/Library/Fonts/Palatino.ttc
bg               = #1a1a2e
bgphoto          = /path/to/photo.jpg
dim              = 50
textcolor        = #e8e8f0
citecolor        = #8888bb
citefont         =
quotes           = yes
citesize         = 0
citescale        = 100
citestyle        = dash
citeplacement    = bottom
citebibleversion = yes
citeshadow       = no
citealign        = center
citepanel        = independent
textsize         = 0
maxtextsize      = 0
textscale        = 100
textpanel        = 0
textpanelcolor   = black
textshadow       = no
shadowmethod     = 1
linespacing      = 0
textoffy         = 0
citeoffy         = 0
reservetop       = 0
reserveright     = 0
reservebottom    = 0
reserveleft      = 0
```

Supported keys: `bv`, `width`, `height`, `font`, `bg`, `bgphoto`, `dim`, `textcolor`, `citecolor`, `citefont`, `quotes`, `citesize`, `citescale`, `citestyle`, `citeplacement`, `citebibleversion`, `citeshadow`, `citealign`, `citepanel`, `textsize`, `maxtextsize`, `textscale`, `textpanel`, `textpanelcolor`, `textpanelrounded`, `textshadow`, `shadowmethod`, `textoutline`, `textoutlinecolor`, `linespacing`, `textoffy`, `citeoffy`, `reservetop`, `reserveright`, `reservebottom`, `reserveleft`

## Bible Translations

- **KJV**: King James Version — `BibleKJV.txt` (downloaded from https://openbible.com/textfiles/kjv.txt)
- **BSB**: Berean Standard Bible — `BibleBSB.txt` (downloaded from https://bereanbible.com/bsb.txt)
- **WEB**: World English Bible — `BibleWEB.txt` (downloaded from https://openbible.com/textfiles/web.txt)

If a Bible translation file is not found, the program will prompt you to download it automatically using `curl`.

## Live Preview (bvilive)

`bvilive` is a two-window GUI that renders a verse in real time as you type or navigate.

**Requirements:** Python 3 + Pillow (`pip install pillow`)

```bash
./bvilive
```

**Control window:**

| Control | Action |
|---|---|
| Reference field | Type any reference; image updates after a short pause |
| `↑` / `↓` arrow keys | Previous / next verse |
| `Page Up` / `Page Down` | Previous / next chapter |
| ◀◀ Chap / ▶▶ Chap buttons | Step one chapter |
| ◀ Verse / Verse ▶ buttons | Step one verse |
| Version radio buttons | Switch KJV / BSB / WEB instantly |
| Max text pt | Cap auto-fit font size (e.g. `140` for typical verse size) |
| Text scale % | Scale text area (e.g. `75` for smaller text) |
| Text shadow (0–10) | Intensity of drop shadow behind verse text; 0 = off |
| Cite style | `dash` / `parens` / `plain` / `none` |
| Cite placement | `bottom` (fixed near edge) or `below` (just under verse text) |
| Cite shadow (0–10) | Intensity of drop shadow behind citation text; 0 = off |
| Cite align | `center` / `left` / `right` |
| Cite panel | `independent` / `coverbottom` / `none` — how the text panel relates to the bottom citation |
| Text off Y | Nudge verse text up (negative) or down (positive) |
| Cite off Y | Nudge citation toward bottom (positive) or away (negative) |
| Reserve % T/R/B/L | Reserve a percentage of each side; verse text centers in the remaining area |
| Shadow method | `1` = soft Gaussian blur, `2` = hard offset copy (drop-down) |
| Quotes checkbox | Toggle curly quotes |

Chapter and verse boundaries are respected using the loaded Bible file — navigation wraps correctly at chapter ends and beginnings.

## Files

- `bvi.cpp` — Source code
- `bvilive` — Live two-window preview script (Python 3 + Pillow)
- `BibleKJV.txt` — KJV Bible text (shared with gospel, downloaded on first use)
- `BibleBSB.txt` — BSB Bible text (shared with gospel, downloaded on first use)
- `BibleWEB.txt` — WEB Bible text (shared with gospel, downloaded on first use)
- `.verselumen` — Shared config file; `[bvi]` section created by `--saveconfig`
