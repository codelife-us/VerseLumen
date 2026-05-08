# bv

A lightweight C++ command-line tool for looking up Bible verses and outputting them to stdout. A stripped-down companion to `gospel`, focused purely on verse reference lookup with no tract output, no file writing, and no PDF generation.

## Building

**macOS / Linux:**
```bash
g++ -std=c++11 -o bv bv.cpp
```

**Windows (MSVC):**
```bat
cl /EHsc /std:c++17 /utf-8 bv.cpp /Fe:bv.exe
```

**Windows (MinGW / MSYS2):**
```bash
g++ -std=c++11 -o bv.exe bv.cpp
```

## Usage

Show help:
```bash
./bv -h
./bv --help
```

Show version:
```bash
./bv -v
./bv --version
```

Look up a single verse (positional or named):
```bash
./bv "John 3:16"
./bv --ref="John 3:16"
```

Look up multiple verses (comma-separated):
```bash
./bv "John 3:16,Romans 5:8"
./bv --ref="John 3:16,Romans 5:8"
./bv --ref="Romans 3:23,Romans 6:23,Romans 10:9-10"
```

Specify a Bible version:
```bash
./bv "John 3:16" -bv=BSB
./bv -bv=BSB --ref="John 3:16"
./bv --bibleversion=WEB --ref="Romans 8:1"
```

Look up a verse range:
```bash
./bv --ref="Romans 8:9-11"
```

Look up a cross-chapter verse range:
```bash
./bv --ref="Judges 7:21-8:5"
```

Look up from a verse to end of chapter (omit ending number after dash):
```bash
./bv --ref="Romans 8:20-"
```

Look up an entire chapter:
```bash
./bv --ref="Romans 8"
./bv --ref="John 3" -bv=WEB
```

Shorthand multi-reference (book name carries forward):
```bash
./bv --ref="Psalm 7, 27"          # same as "Psalm 7, Psalm 27"
./bv --ref="1 John 4:9, 19, 5:1"  # same as "1 John 4:9, 1 John 4:19, 1 John 5:1"
```

Show verse numbers:
```bash
./bv --ref="Romans 8" --versenumbers
./bv --ref="Romans 8" -vn
```

Start each verse on its own line:
```bash
./bv --ref="Romans 8" --versenewline
./bv --ref="Romans 8" -vn -vnl
```

Print book and chapter as a header for full chapters:
```bash
./bv --ref="Romans 8" --chapterheader
./bv --ref="Romans 8" -ch -vn -vnl
```

Wrap verse text in curly quotes:
```bash
./bv --ref="John 3:16" --versequotes
```

Italicize verse text (useful when piping to markdown):
```bash
./bv --ref="John 3:16" --italic
```

Open the reference on esv.org in the browser (no prompt, verse output suppressed):
```bash
./bv --ref="John 3:16" -e
./bv --ref="John 3:16" -esv
./bv --ref="John 3:16" --openesv
```

Open the reference on biblegateway.com in the browser (no prompt, verse output suppressed):
```bash
./bv --ref="John 3:16" -g
./bv --ref="John 3:16" --opengw
```

> Note: Bible Gateway does not carry BSB, so `-bv=BSB` is treated as WEB when using `--opengw`. Any other Bible Gateway version code (e.g. `NLT`, `ESV`, `NASB`) is passed through as-is, even if unsupported by `bv` locally.

## Daily Reading Plans

Look up today's reading from the default (Chronological) plan:
```bash
./bv -d
./bv --day
```

Look up a specific day's reading:
```bash
./bv -d=42
./bv --day=42
```

Select a reading plan with `--plan=`:
```bash
./bv --plan=Chronological --day       # default
./bv --plan=Sequential --day          # also: Canonical, Straight Through
./bv --plan="Old and New Testament" --day   # also: OTNT, OT and NT
```

Print only the reference string (no verse text) — useful for scripting:
```bash
./bv --day --refonly
./bv --day=42 --refonly
```

Open today's reading on esv.org:
```bash
./bv -d -e
./bv --day -esv
./bv --day=42 --plan=Sequential -esv
```

Open today's reading on biblegateway.com:
```bash
./bv -d -g
./bv --day --opengw
./bv --day=42 --plan=Sequential -g
```

## Citation Styles

Control how the reference appears after the verse text with `--refstyle=`:

```bash
./bv --ref="John 3:16" --refstyle=1   # default
./bv --ref="John 3:16" --refstyle=2
./bv --ref="John 3:16" --refstyle=3
./bv --ref="John 3:16" --refstyle=4
```

| Style | Format | Example |
|-------|--------|---------|
| 1 (default) | Citation on its own line below | `For God so loved...`<br>`— John 3:16 (KJV)` |
| 2 | Inline, separated by a dash | `For God so loved... - John 3:16 (KJV)` |
| 3 | Reference in parentheses (no version) | `For God so loved... (John 3:16)` |
| 4 | Reference and version in parentheses | `For God so loved... (John 3:16 (KJV))` |

## Copying Output

Pipe to the clipboard:

**macOS:**
```bash
./bv --ref="John 3:16" | pbcopy
```

**Linux:**
```bash
./bv --ref="John 3:16" | xclip -selection clipboard
```

**Windows:**
```bat
bv --ref="John 3:16" | clip
```

## Configuration File

`bv` reads defaults from the `[bv]` section of `.verselumen` in the current directory. Command-line arguments always override the config file.

Supported keys: `bv`, `refstyle`, `versequotes`, `plan`

Save current settings as defaults:
```bash
./bv -bv=BSB --refstyle=2 --plan=Sequential --saveconfig
```

This writes a `[bv]` section in `.verselumen` like:
```
[bv]
bv          = BSB
refstyle    = 2
versequotes = 0
plan        = Sequential
```

Print effective settings and exit:
```bash
./bv --showconfig
./bv -bv=WEB --showconfig
```

To reset to built-in defaults, delete the `[bv]` section from `.verselumen` or remove the file entirely:
```bash
rm .verselumen
```

## Bible Translations

- **KJV**: King James Version — `BibleKJV.txt` (downloaded from https://openbible.com/textfiles/kjv.txt)
- **BSB**: Berean Standard Bible — `BibleBSB.txt` (downloaded from https://bereanbible.com/bsb.txt)
- **WEB**: World English Bible — `BibleWEB.txt` (downloaded from https://openbible.com/textfiles/web.txt)

Bible `.txt` files are looked for first in the current directory, then in your home directory (`$HOME` on macOS/Linux, `%USERPROFILE%` on Windows). If not found, `bv` will prompt to download the file automatically using `curl`.

## Requirements

- C++11 or later
- Standard C++ library
- `curl` (for automatic Bible file download — built into Windows 10+)
