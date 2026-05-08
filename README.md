# VerseLumen

A collection of tools for reading, sharing, and visualizing Scripture.

All tools share the same three Bible translation files (`BibleKJV.txt`, `BibleBSB.txt`, `BibleWEB.txt`) and look for them first in the current directory, then in `$HOME` on macOS/Linux (`%USERPROFILE%` on Windows). Any tool will prompt to download a missing file automatically via `curl`.

---

## Programs

### [gospelshare](gospelshare.md)

Outputs gospel presentation tracts — biblical frameworks for sharing the Gospel of Jesus Christ. Supports multiple tract styles, Bible versions, and output formats including plain text, Markdown, PDF, and EPUB.

```bash
./gospelshare                            # default tract (Romans Road) in KJV
./gospelshare -tn="Somebody Loves You" -bv=BSB
./gospelshare --output=tract.pdf
./gospelshare --ref="John 3:16"         # look up a verse directly
```

Config file: `.verselumen` `[gospelshare]` | Full docs: [gospelshare.md](gospelshare.md)

---

### [bv](bv.md)

Lightweight Bible verse lookup. Look up any reference, verse range, or full chapter and print it to stdout. Also supports daily reading plans and opening references on ESV.org or Bible Gateway via your browser.

```bash
./bv --ref="John 3:16"
./bv --ref="Romans 8:28-" -bv=BSB       # verse to end of chapter
./bv --ref="Romans 8" -vn               # full chapter with verse numbers
./bv -d                                 # today's reading from the Chronological plan
./bv --ref="John 3:16" -e              # open on ESV.org
```

Config file: `.verselumen` `[bv]` | Full docs: [bv.md](bv.md)

---

### [bvi](bvi.md)

Renders a Bible verse (or custom text) as a JPEG image. Auto-fits the font size to the canvas. Supports solid color or photo backgrounds, citation styles, drop shadows, and semi-transparent text panels.

```bash
./bvi "John 3:16"
./bvi "Philippians 4:6-7" -bv=BSB --output=phil.jpg
./bvi "John 3:16" --bgphoto=sunset.jpg --dim=40
./bvi --text="He is risen." --bg=black --textcolor=white
```

Config file: `.verselumen` `[bvi]` | Full docs: [bvi.md](bvi.md)

---

### bvilive

A two-window live preview GUI for `bvi`. Type a reference and the image updates in real time. Navigate by verse or chapter with arrow keys; switch Bible versions with radio buttons.

```bash
./bvilive
```

Requires Python 3 + Pillow (`pip install pillow`). Documented in [bvi.md](bvi.md).

---

### [biblereader](biblereader.md)

Opens a full Bible reader in your browser via a local HTTP server. Click any verse to copy its reference (or text) to the clipboard. The last selected reference is printed to stdout on quit, making it composable with other tools.

```bash
./biblereader
./biblereader -bv=BSB
./bvi "$(./biblereader)"               # pick a verse in browser, render as image
```

Full docs: [biblereader.md](biblereader.md)

---

### [versepick](versepick.md)

Terminal arrow-key Bible browser. Navigate books → chapters → verses; press Enter to copy the reference (or verse text) to the clipboard.

```bash
./versepick
./versepick --verse -bv=BSB            # copy verse text instead of reference
./bvi "$(./versepick)"                 # pick a verse, render as image
```

Full docs: [versepick.md](versepick.md)

---

### [colorpick](colorpick.md)

Interactive terminal color picker. Navigate a hue × saturation grid with arrow keys, adjust brightness, and press Enter. Prints the chosen color as `#RRGGBB` to stdout. The TUI draws to stderr so the result composes cleanly in shell substitution.

```bash
./bvi "John 3:16" --bg=$(./colorpick) --textcolor=$(./colorpick)
```

Full docs: [colorpick.md](colorpick.md)

---

### [fontlist](fontlist.md)

Browser-based font browser for macOS. Scans system font directories, shows a searchable grid of previews, and prints the selected font path to stdout on exit.

```bash
./fontlist --sample="For God so loved the world"
./bvi "John 3:16" --font="$(./fontlist)"
```

Full docs: [fontlist.md](fontlist.md)

---

### [day](day.md)

Prints the current day of the year and opens a YouTube search for today's video in the browser.
Update the .day file for your own custom daily query string.

```bash
./day -d             # print today's day of the year number only, same with no options
./day && ./bv -d     # open YouTube search and print today's reading plan
./day -h             # show help for the day command line program
```

Full docs: [day.md](day.md)

---

## Shared Bible Files

All tools use the same translation files:

| Version | File | Source |
|---------|------|--------|
| KJV (default) | `BibleKJV.txt` | openbible.com |
| BSB | `BibleBSB.txt` | bereanbible.com |
| WEB | `BibleWEB.txt` | openbible.com |

Files are looked up in the current directory first, then `$HOME` on macOS/Linux (`%USERPROFILE%` on Windows). Any tool will prompt to download a missing file automatically using `curl`.

---

## Common Workflows

**Pick a verse in the browser and render it as an image:**
```bash
./bvi "$(./biblereader)"
```

**Browse interactively in the terminal and render:**
```bash
./bvi "$(./versepick)"
```

**Pick colors interactively for a verse image:**
```bash
./bvi "John 3:16" --bg=$(./colorpick) --textcolor=$(./colorpick) --citecolor=$(./colorpick)
```

**Pick a font from the browser and use it in an image:**
```bash
./bvi "John 3:16" --font="$(./fontlist)"
```

**Open today's search on youtube and view today's reading plan from esv.org in the browser:**
```bash
./day && ./bv -d -e
```

**Export a gospel tract as PDF and print it:**
```bash
./gospelshare --output=tract.pdf --print
```
