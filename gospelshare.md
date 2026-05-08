# gospelshare

A C++ program that displays various gospel presentation tracts, biblical frameworks for sharing the Gospel of Jesus Christ. The program presents different gospel presentations with biblical explanations and references. The `--ref` option outputs Bible references directly without a tract.

## Features

- Support for multiple gospel presentation tracts/styles
- Currently includes four tracts: "The Romans Road" (default), "Somebody Loves You", "Have A Good Day", and "Are You Ready"
- Easy to add new tracts by extending the code
- Display tract content with biblical explanations
- Support for multiple Bible translations:
  - **KJV** (King James Version) - default
  - **BSB** (Berean Standard Bible)
  - **WEB** (World English Bible)
- Look up individual Bible verses, verse ranges, verse-to-end-of-chapter, or entire chapters directly with `--ref`
- Multiple references supported using comma separation
- Optional verse numbers prefixed to each verse with `--versenumbers` / `-vn`
- Each verse on its own line with `--versenewline` / `-vnl`
- Optional book and chapter header when outputting a full chapter with `--chapterheader` / `-ch`
- Configurable citation style: new line, inline, or parentheses (`--refstyle=1–4`)
- Optional curly quotes around verse text with `--versequotes`
- Italics for verse output opt-in with `--italic` (tract inline refs stay italic by default)
- Shorthand multi-reference: `"Psalm 7, 27"` is the same as `"Psalm 7, Psalm 27"`
- Save output to a file with `--output=`, including PDF and EPUB via pandoc
- PDF font control: custom font (`--pdffont=`), size as percentage (`--pdffontsize=`), and margin (`--pdfmargin=`)
- Print PDF directly to printer with `--print`
- EPUB output with embedded title metadata and optional cover image via `--titlegraphic`
- EPUB automatically appends `gospelshare_epub_add.txt` if present, with QR code generated from any URL on the last line (requires `qrencode`)
- Batch export every tract in every Bible version as `.txt`, `.md`, `.pdf`, and `.epub` with `--outputall`
- Command-line configurable tract name and Bible version
- Invalid command line options are reported with an error
- Auto-prompts to download the Bible translation file if not found
- Persistent config file (`.verselumen` `[gospelshare]` section) to save default settings with `--saveconfig`
- View effective settings (config + command-line) with `--showconfig`

## Building

**macOS / Linux:**
```bash
g++ -std=c++11 -o gospelshare gospelshare.cpp
```

**Windows (MSVC):**
```bat
cl /EHsc /std:c++17 /utf-8 gospelshare.cpp /Fe:gospelshare.exe
```

**Windows (MinGW / MSYS2):**
```bash
g++ -std=c++11 -o gospelshare.exe gospelshare.cpp
```

## Usage

Show help information:
```bash
./gospelshare -h
./gospelshare --help
```

Show version information:
```bash
./gospelshare -v
./gospelshare --version
```

Run with default KJV translation and default tract:
```bash
./gospelshare
```

Specify a different Bible version:
```bash
./gospelshare -bv=BSB
./gospelshare -bv=WEB
./gospelshare --bibleversion=BSB
```

Specify a different tract presentation:
```bash
./gospelshare -tn="The Romans Road"
./gospelshare --tractname="The Romans Road"
```

Combine tract and Bible version options:
```bash
./gospelshare -tn="The Romans Road" -bv=BSB
./gospelshare --tractname="The Romans Road" --bibleversion=WEB
```

Output as Markdown:
```bash
./gospelshare --outputtype=md > romansroad.md
```

Look up a single Bible reference directly (bypasses tract output):
```bash
./gospelshare --ref="John 3:16"
./gospelshare --ref="Romans 10:9-10" -bv=BSB
```

Look up multiple references using a comma to separate each:
```bash
./gospelshare --ref="John 3:16,Romans 5:8"
./gospelshare --ref="Romans 3:23,Romans 6:23,Romans 10:9-10"
```

Look up from a verse to the end of the chapter (omit the ending number after the dash):
```bash
./gospelshare --ref="Romans 8:20-"
./gospelshare --ref="John 3:16-" -bv=BSB
```

Look up an entire chapter:
```bash
./gospelshare --ref="Romans 8"
./gospelshare --ref="John 3" -bv=WEB
```

Look up multiple references using shorthand (book name carries forward):
```bash
./gospelshare --ref="Psalm 7, 27"          # same as "Psalm 7, Psalm 27"
./gospelshare --ref="1 John 4:9, 19, 5:1"  # same as "1 John 4:9, 1 John 19, 1 John 5:1"
```

Show verse numbers prefixed to each verse:
```bash
./gospelshare --ref="Romans 8" --versenumbers
./gospelshare --ref="John 3:16,Romans 5:8" -vn
```

Start each verse on its own line:
```bash
./gospelshare --ref="Romans 8" --versenewline
./gospelshare --ref="Romans 8" -vn -vnl
```

Print the book and chapter as a header when outputting a full chapter:
```bash
./gospelshare --ref="Romans 8" --chapterheader
./gospelshare --ref="Romans 8" -ch -vn -vnl
```

Wrap verse text in curly quotes:
```bash
./gospelshare --ref="John 3:16" --versequotes
```

Change the citation style (default is style 1):
```bash
# Style 1 (default): citation on its own line below the verse
./gospelshare --ref="John 3:16" --refstyle=1

# Style 2: citation inline, separated by a dash
./gospelshare --ref="John 3:16" --refstyle=2

# Style 3: reference in parentheses (no version)
./gospelshare --ref="John 3:16" --refstyle=3

# Style 4: reference and version in parentheses
./gospelshare --ref="John 3:16" --refstyle=4
```

Style 1 output example:
```
For God so loved the world...
— John 3:16 (KJV)
```

Style 2 output example:
```
For God so loved the world... - John 3:16 (KJV)
```

Style 3 output example:
```
For God so loved the world... (John 3:16)
```

Style 4 output example:
```
For God so loved the world... (John 3:16 (KJV))
```

Italicize verse output in `--ref` mode (off by default):
```bash
./gospelshare --ref="John 3:16" --italic
```

Save output to a file:
```bash
./gospelshare --output=romansroad.txt
./gospelshare --outputtype=md --output=romansroad.md
```

Save output as PDF (requires pandoc):
```bash
./gospelshare --output=romansroad.pdf
./gospelshare --ref="John 3:16,Romans 5:8" --output=verses.pdf
```

Print the PDF directly after generating:
```bash
./gospelshare --output=verse.pdf --print
./gospelshare --ref="John 3:16" --output=verse.pdf --print
```

Save output as EPUB (requires pandoc):
```bash
./gospelshare --output=romansroad.epub
./gospelshare -tn="Somebody Loves You" --output=sly.epub
```

Save EPUB with a cover image (auto-named `{tractname}_1.jpg` by default):
```bash
./gospelshare --output=romansroad.epub --titlegraphic
./gospelshare --output=romansroad.epub --titlegraphic=mycover.jpg
```

The tract name is embedded as the EPUB title metadata. If `gospelshare_epub_add.txt` exists in the current directory, its contents are appended to the EPUB. If the last line of that file is a URL, a QR code is generated and included (requires `qrencode`):
```bash
brew install qrencode   # macOS
apt install qrencode    # Linux
```

Batch export all tracts in all Bible versions as `.txt`, `.md`, `.pdf`, and `.epub`:
```bash
./gospelshare --outputall
./gospelshare --outputall --titlegraphic   # include cover images in EPUBs
```

Files are saved in the current directory using the naming pattern `tractname_VERSION.ext` (tract name lowercased, spaces removed), for example:
```
theromansroad_KJV.txt
theromansroad_KJV.md
theromansroad_KJV.pdf
theromansroad_KJV.epub
haveagoodday_BSB.txt
...
```

Bible versions are skipped silently if their translation file is not present. PDF and EPUB generation require pandoc (EPUB does not need a LaTeX engine). PDF font and margin options (`--pdffont=`, `--pdfmargin=`, `--pdffontsize=`) apply to all generated PDFs.

To set or fix the default printer:

**macOS / Linux:**
```bash
lpoptions -d YourPrinterName   # set default printer
lpstat -p                      # list available printers
rm ~/.cups/lpoptions           # clear stale printer config if lpr errors occur
```

**Windows:** printing uses PowerShell `Start-Process -Verb Print` with your default PDF viewer.

Control the PDF margin (default is `0.5in`):
```bash
./gospelshare --output=romansroad.pdf --pdfmargin=0.75in
./gospelshare --output=romansroad.pdf --pdfmargin=2cm
```

Set a custom PDF font size as a percentage of the default (11pt base):
```bash
./gospelshare --output=romansroad.pdf --pdffontsize=120    # 120% → 13pt
./gospelshare --output=romansroad.pdf --pdffontsize=150    # 150% → 17pt
```

Color specific words or phrases in PDF output using HTML span tags in the tract data:
```cpp
"<span style=\"color: red;\">word</span>"        // named color
"<span style=\"color: #C0392B;\">word</span>"    // hex color
```
Colors are converted to LaTeX `\textcolor` automatically when generating a PDF.

Set a custom PDF font (requires xelatex):
```bash
./gospelshare --output=romansroad.pdf --pdffont="Georgia"
./gospelshare --output=romansroad.pdf --pdffont="Times New Roman"
```

Default fonts by platform: `Palatino` (macOS), `Palatino Linotype` (Windows), none/system default (Linux).

> **Linux note:** Palatino is not installed by default on Linux. The closest available equivalent included with texlive is `TeX Gyre Pagella`:
> ```bash
> ./gospelshare --output=romansroad.pdf --pdffont="TeX Gyre Pagella"
> ```

Install xelatex if needed:
```bash
# macOS / Linux
sudo tlmgr install xetex

# Windows
winget install MiKTeX.MiKTeX   # includes xelatex
```

If pandoc or a LaTeX engine is not installed the program will tell you how to install them and show the manual alternative:
```bash
./gospelshare --outputtype=md | pandoc -f markdown -o output.pdf
```

Install pandoc and a LaTeX engine:
```bash
# macOS
brew install pandoc && brew install --cask basictex
sudo tlmgr update --self

# Linux
apt install pandoc texlive

# Windows
winget install JohnMacFarlane.Pandoc
winget install MiKTeX.MiKTeX
```

> **macOS note:** After installing basictex, open a new Terminal window before running gospelshare. The `pdflatex` command is added to your PATH at login and will not be found in an existing Terminal session.

> **Windows note:** `curl` is built into Windows 10 and later and is used to download Bible translation files automatically. On Windows, `--print` uses PowerShell to send the PDF to your default printer.

## Configuration File

gospelshare reads defaults from the `[gospelshare]` section of `.verselumen` in the current directory. Command-line arguments always override the config file.

Supported keys: `bv`, `tractname`, `refstyle`, `pdfmargin`, `pdffont`, `pdffontsize`, `outputtype`

Save current settings as new defaults:
```bash
./gospelshare -bv=BSB --refstyle=2 --saveconfig
```

This writes a `[gospelshare]` section in `.verselumen` like:
```
[gospelshare]
bv          = BSB
tractname   = The Romans Road
refstyle    = 2
pdfmargin   = 0.5in
pdffont     = Palatino
pdffontsize = 100
```

Print the effective settings (config file + any command-line overrides) and exit:
```bash
./gospelshare --showconfig
./gospelshare -bv=WEB --showconfig    # preview what would be used without running
```

To reset to built-in defaults, delete the `[gospelshare]` section from `.verselumen` or remove the file entirely:
```bash
rm .verselumen
```

## Copying Output

The program output can be easily copied to the clipboard using piping:

**macOS:**
```bash
./gospelshare | pbcopy
```

**Linux:**
```bash
./gospelshare | xclip -selection clipboard
# or
./gospelshare | xsel -b
```

**Windows:**
```bat
gospelshare | clip
```

This is useful for sharing gospel presentations or copying the content for use in documents, emails, or other applications.

## Bible Translations

- **KJV**: King James Version — `BibleKJV.txt` (downloaded from https://openbible.com/textfiles/kjv.txt)
- **BSB**: Berean Standard Bible — `BibleBSB.txt` (downloaded from https://bereanbible.com/bsb.txt)
- **WEB**: World English Bible — `BibleWEB.txt` (downloaded from https://openbible.com/textfiles/web.txt)

If a Bible translation file is not found, the program will prompt you to download it automatically using `curl`.

## Files

- `gospelshare.cpp` - Main program source code
- `.verselumen` - Shared config file; `[gospelshare]` section created by `--saveconfig`
- `BibleKJV.txt` - KJV Bible text (downloaded on first use)
- `BibleBSB.txt` - BSB Bible text (downloaded on first use)
- `BibleWEB.txt` - WEB Bible text (downloaded on first use)

## Requirements

- C++11 or later
- Standard C++ library
- `curl` (for automatic Bible file download — built into Windows 10+)
- `pandoc` (for PDF and EPUB output):
  - macOS/Linux/Windows: see [pandoc.org](https://pandoc.org/installing.html)
- A LaTeX engine (for PDF output only):
  - macOS: `basictex` via Homebrew
  - Linux: `texlive` via apt
  - Windows: `MiKTeX` via winget
- `xelatex` / `xetex` (for custom PDF fonts via `--pdffont=`)
- `qrencode` (optional, for QR code in EPUB from `gospelshare_epub_add.txt`):
  - macOS: `brew install qrencode`
  - Linux: `apt install qrencode`
