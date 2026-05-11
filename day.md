# day

A small C++ utility that prints the current day of the year (Jan 1 = 1).

## Building

```bash
g++ -std=c++11 -o day day.cpp
```

## Usage

```bash
./day                  # print day number of today only, same as day -d
./day -r               # print Bible reference for today only
./day -p               # print day number of today, date, and Bible reference
./day -y               # print day number and open YouTube search
./day -d=4/30/2026 -r  # print Bible reference only for a specific date
```

## Options

| Option | Description |
|--------|-------------|
| `-d=N`, `--day=N` | Use day N instead of today |
| `-d=mm/dd/yyyy` | Use a date instead of a day number (4-digit or 2-digit year) |
| `-y`, `--youtube` | Open YouTube Bible Recap search |
| `-a`, `--app` | Open YouTube in the browser set by `browser=` in `.verselumen`; macOS default (no setting): Google Chrome; implies `-y` |
| `-q=TEXT`, `--query=TEXT` | Override the YouTube search query (`{day}` = day number); implies `-y` |
| `-r`, `--refonly` | Print Bible reference only |
| `-p`, `--plan` | Print day number, date, and Bible reference |
| `-c`, `--csv` | Output as CSV: `day,date,"reference"` |
| `-t`, `--tab` | Output as TSV: `day<TAB>date<TAB>reference` |
| `-v`, `--version` | Print version |
| `-h`, `--help` | Show help |

## Config file (.day)

Create a `.day` file in the current directory or `$HOME` (macOS/Linux) / `%USERPROFILE%` (Windows) to set a default YouTube search query. The first non-blank, non-comment line is used. Lines starting with `#` are ignored.

```
# .day — default YouTube search query for day
Day {day} The Bible Recap
```

Query priority: `-q=` flag → `.day` in current dir → `.day` in `$HOME` / `%USERPROFILE%` → built-in default (`Day {day} The Bible Recap`).

## Config file (.verselumen)

The `.verselumen` file (current directory or `$HOME`) holds shared settings for VerseLumen tools. `day` reads the following global key (no section header required):

| Key | Description | Default |
|-----|-------------|---------|
| `browser` | Browser used by `-a` | macOS: `Google Chrome`; others: system default |

Example:
```
# .verselumen
browser=Firefox
```

The value means different things per platform:

| Platform | Interpretation | Example values |
|----------|---------------|----------------|
| macOS | App name passed to `open -a` | `Google Chrome`, `Firefox`, `Safari`, `Arc` |
| Windows | Executable name or path passed to `start` | `chrome`, `firefox`, `msedge` |
| Linux | Command name invoked directly | `google-chrome`, `firefox`, `chromium` |

## Examples

Print today's day number:
```bash
./day
```

Open YouTube for today's Bible Recap:
```bash
./day -y
```

Open YouTube in Google Chrome (or the browser set in `.verselumen`):
```bash
./day -a
./day -d=203 -a
```

Open YouTube for a specific day or date:
```bash
./day -d=203 -y
./day -d=4/30/2026 -y
./day -d=12/25/25 -y
```

Custom search query (opens YouTube automatically):
```bash
./day -q="Day {day} The Bible Recap"
./day --query="Day {day} The Bible Recap"
```

Print today's Bible reference only:
```bash
./day -r
./day -d=3/21/2026 -r
```

Print today's reading plan (day number, date, reference):
```bash
./day -p
```

Output today as a CSV row:
```bash
./day --csv
./day -d=203 --csv
```

Output today as a tab-delimited row:
```bash
./day --tab
./day -d=203 --tab
```

## Composing with other tools

```bash
./bv -d=$(./day)          # pipe day number into bv
./day -r && ./day -y      # print reference then open YouTube
```
