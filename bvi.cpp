// MIT License
//
// Copyright (c) 2026 Code Life
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

////////////////////////////////////////////////////////////////////////////////
// bvi.cpp  (Bible Verse Image)
// Renders a Bible verse reference to a JPEG image with auto-fitted text.
// Requires ImageMagick (convert).

#include <iostream>
#include <string>
#include <algorithm>
#include <map>
#include <fstream>
#include <vector>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#define pclose _pclose
#endif

using namespace std;

// runSystem / runPopen — platform wrappers
//
// On Windows we bypass cmd.exe entirely:
//   • runSystem uses CreateProcess directly, eliminating ~0.3-0.5 s of
//     shell-startup overhead per ImageMagick call and preserving UTF-8
//     characters (em dash, curly quotes) that cmd.exe would mangle via ANSI.
//   • runPopen still uses _wpopen (cmd.exe) because piped output capture
//     needs the shell infrastructure; it is only used for optional features.
#ifdef _WIN32
static int runSystem(const string& utf8cmd) {
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8cmd.c_str(), -1, nullptr, 0);
    if (n <= 0) return -1;
    wstring wc(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8cmd.c_str(), -1, &wc[0], n);
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, wc.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return 1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}
static FILE* runPopen(const string& utf8cmd, const char* mode) {
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8cmd.c_str(), -1, nullptr, 0);
    if (n <= 0) return nullptr;
    wstring wc(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8cmd.c_str(), -1, &wc[0], n);
    wchar_t wm[8] = {};
    for (size_t i = 0; mode[i] && i < 7; ++i) wm[i] = (wchar_t)mode[i];
    return _wpopen(wc.c_str(), wm);
}
#else
static int   runSystem(const string& cmd)                  { return system(cmd.c_str()); }
static FILE* runPopen(const string& cmd, const char* mode) { return popen(cmd.c_str(), mode); }
#endif

#ifdef _WIN32
#define HOME_ENV "USERPROFILE"
#else
#define HOME_ENV "HOME"
#endif

const string BVI_VERSION = "1.3";
const string CONFIG_FILE = ".verselumen";
const string SECTION     = "bvi";

// ── Config file (.verselumen, [bvi] section) ─────────────────────────────────

// Read key=value pairs from the [bvi] section of CONFIG_FILE.
// Falls back to $HOME/.verselumen if not found in current directory.
map<string, string> loadConfig() {
    map<string, string> cfg;
    ifstream f(CONFIG_FILE);
    if (!f.good()) {
        const char* home = getenv(HOME_ENV);
        if (!home) return cfg;
        ifstream fh(string(home) + "/" + CONFIG_FILE);
        if (!fh.good()) return cfg;
        swap(f, fh);
    }
    string line;
    bool inSection = false;
    while (getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t s = line.find_first_not_of(" \t");
        if (s == string::npos) continue;
        if (line[s] == '[') {
            size_t e = line.find(']', s);
            inSection = (e != string::npos && line.substr(s + 1, e - s - 1) == SECTION);
            continue;
        }
        if (!inSection || line[s] == '#') continue;
        line = line.substr(s);
        size_t eq = line.find('=');
        if (eq == string::npos) continue;
        string key = line.substr(0, eq);
        string val = line.substr(eq + 1);
        size_t ke = key.find_last_not_of(" \t");
        if (ke != string::npos) key = key.substr(0, ke + 1);
        size_t vs = val.find_first_not_of(" \t");
        val = (vs != string::npos) ? val.substr(vs) : "";
        if (!key.empty()) cfg[key] = val;
    }
    return cfg;
}

// Return cfg[key] if present, otherwise defaultVal.
string cfgGet(const map<string, string>& cfg, const string& key, const string& defaultVal) {
    auto it = cfg.find(key);
    return (it != cfg.end()) ? it->second : defaultVal;
}

// ── Bible loading (same logic as gospel.cpp) ─────────────────────────────────

map<string, string> bibleVerses;

map<string, string> loadBible(const string& filename) {
    map<string, string> verses;
    ifstream file(filename);
    string line;
    while (getline(file, line)) {
        // Strip UTF-8 BOM if present
        if (line.size() >= 3 &&
            (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB &&
            (unsigned char)line[2] == 0xBF)
            line = line.substr(3);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t tab = line.find('\t');
        if (tab == string::npos) continue;
        string ref = line.substr(0, tab);
        if (ref.find(':') == string::npos) continue;
        verses[ref] = line.substr(tab + 1);
    }
    return verses;
}

// Looks up a single verse, range (8:9-10), or verse-to-end (8:20-).
string lookupVerses(const string& reference) {
    size_t colon = reference.rfind(':');
    if (colon != string::npos) {
        size_t dash = reference.find('-', colon);
        if (dash != string::npos) {
            string bookChapter = reference.substr(0, colon + 1);
            int startVerse = stoi(reference.substr(colon + 1, dash - colon - 1));
            int endVerse;
            string afterDash = reference.substr(dash + 1);
            if (afterDash.empty()) {
                endVerse = 0;
                for (const auto& entry : bibleVerses) {
                    if (entry.first.compare(0, bookChapter.size(), bookChapter) == 0) {
                        int v = stoi(entry.first.substr(bookChapter.size()));
                        if (v > endVerse) endVerse = v;
                    }
                }
                if (endVerse == 0) {
                    cerr << "Warning: no verses found for '" << reference << "'.\n";
                    return "";
                }
            } else {
                endVerse = stoi(afterDash);
            }
            string combined;
            for (int v = startVerse; v <= endVerse; ++v) {
                string key = bookChapter + to_string(v);
                auto it = bibleVerses.find(key);
                if (it != bibleVerses.end()) {
                    if (!combined.empty()) combined += " ";
                    combined += it->second;
                } else {
                    cerr << "Warning: verse not found: " << key << "\n";
                }
            }
            return combined;
        }
    }
    // Single verse
    auto it = bibleVerses.find(reference);
    if (it != bibleVerses.end()) return it->second;
    return "";
}

// ── Helpers ──────────────────────────────────────────────────────────────────

// Build a safe default filename from a reference like "Philippians 4:6-7"
string refToFilename(const string& ref) {
    string s;
    for (char c : ref) {
        if (c == ' ' || c == ':') s += '_';
        else s += c;
    }
    return s + ".jpg";
}

// Wraps s in shell quotes suitable for the current platform.
// On Windows, system() uses cmd.exe, which does not support POSIX single-quote
// quoting — single-quoted text is split on spaces, causing later words to be
// interpreted as filenames by ImageMagick.  Use double-quote style instead.
string shellQuote(const string& s) {
#ifdef _WIN32
    string r = "\"";
    for (char c : s) {
        if (c == '"') r += "\"\"";   // "" is the cmd.exe literal-double-quote escape
        else r += c;
    }
    return r + "\"";
#else
    string r = "'";
    for (char c : s) {
        if (c == '\'') r += "'\\''";
        else r += c;
    }
    return r + "'";
#endif
}

// Returns "magick" if available, "convert" if not, or "" if neither found.
string detectIM() {
#ifdef _WIN32
    // SearchPathW is an instant filesystem probe — no process spawn needed.
    wchar_t probe[MAX_PATH];
    if (SearchPathW(nullptr, L"magick",  L".exe", MAX_PATH, probe, nullptr)) return "magick";
    if (SearchPathW(nullptr, L"convert", L".exe", MAX_PATH, probe, nullptr)) return "convert";
#else
    if (runSystem("magick -version >/dev/null 2>&1") == 0) return "magick";
    if (runSystem("convert -version >/dev/null 2>&1") == 0) return "convert";
#endif
    return "";
}

void printHelp() {
    cout << "bvi v" << BVI_VERSION << "\n\n";
    cout << "Usage: bvi \"Reference\" [OPTIONS]\n";
    cout << "       bvi --ref=\"Reference\" [OPTIONS]\n";
    cout << "       bvi --text=\"Custom text\" [OPTIONS]\n\n";
    cout << "  Reference formats: \"Book Ch:V\"  \"Book Ch:V-V\"  \"Book Ch:V-\"\n\n";
    cout << "Options:\n";
    cout << "  -h, --help              Show this help message and exit\n";
    cout << "  -ref=REF, --ref=REF     Bible reference (alternative to positional argument)\n";
    cout << "  --text=TEXT             Custom text to render, bypassing Bible lookup\n";
    cout << "  -bv=VERSION             Bible version (KJV, BSB, WEB; default: KJV)\n";
    cout << "  --bibleversion=VERSION  Specify Bible version (KJV, BSB, WEB)\n";
    cout << "  --output=FILE           Output image file (default: <Reference>.jpg)\n";
    cout << "  --width=N               Image width in pixels (default: 1920)\n";
    cout << "  --height=N              Image height in pixels (default: 1080)\n";
    cout << "  --font=FONT             Font name or file path\n";
    cout << "  --bg=COLOR              Background color (default: black)\n";
    cout << "  --bgphoto=FILE          Background photo (jpg/png); overrides --bg\n";
    cout << "  --dim=N                 Darken photo 0-100% (default: 50); ignored without --bgphoto\n";
    cout << "  --textcolor=COLOR       Verse text color (default: white)\n";
    cout << "  --citecolor=COLOR       Citation text color (default: gray60)\n";
    cout << "  --citefont=FONT         Citation font name or path (default: same as verse font)\n";
    cout << "  --quotes                Wrap verse text in \xe2\x80\x9c\xe2\x80\x9d quotation marks\n";
    cout << "  --no-quotes             Remove quotation marks (default)\n";
    cout << "  --citesize=N            Citation font size in points (default: auto ~60pt at 1080p)\n";
    cout << "  --citescale=PCT         Scale citation size to PCT% of auto (e.g. 75); cannot combine with --citesize\n";
    cout << "  --citestyle=STYLE       dash (default): \xe2\x80\x94 Ref (Ver)  |  parens: (Ref, Ver)  |  plain: Ref (Ver)  |  none: omit citation\n";
    cout << "  --citeplacement=WHERE   bottom (default): near bottom edge  |  below: just under verse text\n";
    cout << "  --citebibleversion=VAL  yes (default): include Bible version in citation  |  no/false: omit it\n";
    cout << "  --citeshadow[=N]        Add drop shadow behind citation text; N=1-10 intensity (default 5)\n";
    cout << "  --no-citeshadow         Remove citation drop shadow (default)\n";
    cout << "  --citealign=ALIGN       center (default) | left | right\n";
    cout << "  --citepanel=MODE        When --textpanel and --citeplacement=bottom are both active:\n";
    cout << "                          independent (default): separate narrow panel behind citation\n";
    cout << "                          coverbottom: extend main text panel down to cover citation\n";
    cout << "                          none: no panel behind citation\n";
    cout << "  --textsize=N            Force verse font to exactly N points (absolute; cannot combine with --textscale)\n";
    cout << "  --maxtextsize=N         Cap auto-fit verse font at N points (e.g. 140 for typical verses at 1080p); overrides --textsize\n";
    cout << "  --textscale=PCT         Scale verse text area to PCT% of default (e.g. 75); cannot combine with --textsize/--maxtextsize\n";
    cout << "  --textpanel=N           Semi-transparent panel behind text, N=opacity 1-100 (default: off)\n";
    cout << "  --textpanelcolor=COLOR  Panel color (default: black); any ImageMagick color\n";
    cout << "  --textpanelrounded      Rounded corners on text panel\n";
    cout << "  --no-textpanelrounded   Square corners (default)\n";
    cout << "  --textshadow[=N]        Add drop shadow behind verse text; N=1-10 intensity (default 5)\n";
    cout << "  --no-textshadow         Remove drop shadow (default)\n";
    cout << "  --shadowmethod=N        Shadow style: 1=soft Gaussian blur (default), 2=hard offset copy\n";
    cout << "  --textoutline[=N]       Outline width in pixels around verse text (default 2); stronger emphasis than shadow\n";
    cout << "  --no-textoutline        Remove outline (default)\n";
    cout << "  --textoutlinecolor=C    Outline color (default: black); any ImageMagick color\n";
    cout << "  --linespacing=N         Adjust line spacing: positive=more, negative=less, 0=default\n";
    cout << "  --textoffy=N            Shift verse text vertically; positive=down, negative=up (default 0)\n";
    cout << "  --citeoffy=N            Shift citation vertically; positive=toward bottom edge, negative=away (default 0)\n";
    cout << "  --reserve=SIDE,PCT      Reserve PCT% on SIDE (top/right/bottom/left); repeat for multiple sides\n\n";
    cout << "Config file (.verselumen in current directory or $HOME, [bvi] section):\n";
    cout << "  --saveconfig            Save current settings to .verselumen [bvi] as new defaults\n";
    cout << "  --showconfig            Print current effective settings and exit\n\n";
    cout << "  Supported keys in [bvi]:  bv  width  height  font  bg  bgphoto  dim  textcolor  citecolor  citefont  quotes  citesize  citescale  citestyle  citeplacement  citebibleversion  citeshadow  citealign  citepanel  textsize  maxtextsize  textscale  textpanel  textpanelcolor  textpanelrounded  textshadow  shadowmethod  textoutline  textoutlinecolor  linespacing  textoffy  citeoffy  reservetop  reserveright  reservebottom  reserveleft\n\n";
    cout << "Requires:\n";
    cout << "  ImageMagick  —  brew install imagemagick\n\n";
    cout << "Examples:\n";
    cout << "  bvi \"Philippians 4:6-7\"\n";
    cout << "  bvi --ref=\"John 3:16\" -bv=BSB --output=john316.jpg\n";
    cout << "  bvi \"Romans 8:28\" --bg=\"#1a1a2e\" --textcolor=\"#e0e0e0\" --citecolor=\"#8888aa\"\n";
    cout << "  bvi --text=\"He is risen.\" --citestyle=none --bg=black --textcolor=white\n";
    cout << "  bvi --bg=navy --textcolor=gold --citecolor=lightyellow --saveconfig\n";
    cout << "\nTo list available fonts: magick -list font\n";
    cout << "  Or pass a font file path: --font=\"/path/to/font.ttf\"\n";
}

static bool writeSection(const vector<string>& lines) {
    vector<string> before, after;
    bool inTarget = false, found = false;
    ifstream f(CONFIG_FILE);
    if (f.good()) {
        string line;
        while (getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            size_t s = line.find_first_not_of(" \t");
            if (s != string::npos && line[s] == '[') {
                size_t e = line.find(']', s);
                string sec = (e != string::npos) ? line.substr(s + 1, e - s - 1) : "";
                if (sec == SECTION) { inTarget = true; found = true; continue; }
                else inTarget = false;
            }
            if (inTarget) continue;
            (found ? after : before).push_back(line);
        }
    }
    while (!before.empty() && before.back().empty()) before.pop_back();
    while (!after.empty() && after.front().empty()) after.erase(after.begin());
    ofstream out(CONFIG_FILE);
    if (!out) return false;
    for (const string& l : before) out << l << "\n";
    if (!before.empty()) out << "\n";
    out << "[" << SECTION << "]\n";
    for (const string& l : lines) out << l << "\n";
    if (!after.empty()) { out << "\n"; for (const string& l : after) out << l << "\n"; }
    return true;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // ImageMagick group operators: bash requires \( \) to avoid shell
    // interpretation; Windows (CreateProcess or cmd.exe) uses bare ( ).
#ifdef _WIN32
    const string LP = "(", RP = ")";
#else
    const string LP = "\\(", RP = "\\)";
#endif

    string im = detectIM();

    // ── Load config file first; command-line args override these values ───
    map<string, string> cfg = loadConfig();

#ifdef __APPLE__
    const string defaultFont = "/System/Library/Fonts/Palatino.ttc";
#elif defined(_WIN32)
    const string defaultFont = "C:/Windows/Fonts/pala.ttf";
#else
    const string defaultFont = "DejaVu-Serif";
#endif

    string version    = cfgGet(cfg, "bv",        "KJV");
    string reference;
    string customText;
    string outputFile;
    int imgWidth      = stoi(cfgGet(cfg, "width",     "1920"));
    int imgHeight     = stoi(cfgGet(cfg, "height",    "1080"));
    string font       = cfgGet(cfg, "font",           defaultFont);
    string bgColor    = cfgGet(cfg, "bg",             "black");
    string bgPhoto    = cfgGet(cfg, "bgphoto",        "");
    int    dimPct     = stoi(cfgGet(cfg, "dim",       "50"));
    string textColor  = cfgGet(cfg, "textcolor",      "white");
    string citeColor  = cfgGet(cfg, "citecolor",      "gray60");
    string citeFont   = cfgGet(cfg, "citefont",       "");               // empty = same as verse font
    bool quotes       = cfgGet(cfg, "quotes",         "no") == "yes";
    int citeSizeOvr      = stoi(cfgGet(cfg, "citesize",      "0"));      // 0 = auto
    int citeScalePct     = stoi(cfgGet(cfg, "citescale",     "100"));    // 100 = auto base
    string citeStyle     = cfgGet(cfg, "citestyle",     "dash");         // dash | parens | plain
    string citePlacement = cfgGet(cfg, "citeplacement", "bottom");       // bottom | below
    bool citeBibleVersion = (cfgGet(cfg, "citebibleversion", "yes") != "no" &&
                             cfgGet(cfg, "citebibleversion", "yes") != "false");
    auto parseShadow = [](const string& v) -> int {
        if (v == "yes") return 5;
        if (v == "no" || v.empty()) return 0;
        try { int n = stoi(v); return max(0, min(10, n)); } catch (...) { return 0; }
    };
    int citeShadow       = parseShadow(cfgGet(cfg, "citeshadow", "no"));
    string citeAlign     = cfgGet(cfg, "citealign",     "center");       // center | left | right
    string citePanel     = cfgGet(cfg, "citepanel",     "independent");  // independent | coverbottom | none
    int textSizePt       = stoi(cfgGet(cfg, "textsize",      "0"));      // 0 = off; absolute fixed size
    int maxTextSizePt    = stoi(cfgGet(cfg, "maxtextsize",   "0"));      // 0 = off; cap auto-fit
    int textScalePct     = stoi(cfgGet(cfg, "textscale",     "100"));    // 100 = fill canvas
    int textPanelOpacity = stoi(cfgGet(cfg, "textpanel",     "0"));      // 0 = off
    string textPanelColor = cfgGet(cfg, "textpanelcolor",   "black");
    int textShadow       = parseShadow(cfgGet(cfg, "textshadow", "no"));
    int shadowMethod     = max(1, min(2, stoi(cfgGet(cfg, "shadowmethod", "1"))));
    int textOutline      = max(0, stoi(cfgGet(cfg, "textoutline",      "0")));   // 0 = off
    string textOutlineColor = cfgGet(cfg, "textoutlinecolor", "black");
    bool panelRounded    = cfgGet(cfg, "textpanelrounded",  "no") == "yes";
    int lineSpacing      = stoi(cfgGet(cfg, "linespacing",    "0"));         // 0 = default; positive=more, negative=less
    int textOffsetY      = stoi(cfgGet(cfg, "textoffy",      "0"));         // 0 = default; positive=down, negative=up
    int citeOffsetY      = stoi(cfgGet(cfg, "citeoffy",      "0"));         // 0 = default; positive=toward bottom, negative=away
    int reserveTop       = stoi(cfgGet(cfg, "reservetop",    "0"));
    int reserveRight     = stoi(cfgGet(cfg, "reserveright",  "0"));
    int reserveBottom    = stoi(cfgGet(cfg, "reservebottom", "0"));
    int reserveLeft      = stoi(cfgGet(cfg, "reserveleft",   "0"));

    bool saveConfig  = false;
    bool showConfig  = false;

    // ── Argument parsing ──────────────────────────────────────────────────
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            cout << "bvi v" << BVI_VERSION << "\n";
            return 0;
        } else if (arg.find("-bv=") == 0) {
            version = arg.substr(4);
        } else if (arg.find("--bibleversion=") == 0) {
            version = arg.substr(arg.find('=') + 1);
        } else if (arg.find("--output=") == 0) {
            outputFile = arg.substr(9);
        } else if (arg.find("--width=") == 0) {
            imgWidth = stoi(arg.substr(8));
        } else if (arg.find("--height=") == 0) {
            imgHeight = stoi(arg.substr(9));
        } else if (arg.find("--font=") == 0) {
            font = arg.substr(7);
        } else if (arg.find("--bg=") == 0) {
            bgColor = arg.substr(5);
        } else if (arg.find("--bgphoto=") == 0) {
            bgPhoto = arg.substr(10);
        } else if (arg.find("--dim=") == 0) {
            dimPct = stoi(arg.substr(6));
        } else if (arg.find("--textcolor=") == 0) {
            textColor = arg.substr(12);
        } else if (arg.find("--citecolor=") == 0) {
            citeColor = arg.substr(12);
        } else if (arg.find("--citefont=") == 0) {
            citeFont = arg.substr(11);
        } else if (arg == "--quotes") {
            quotes = true;
        } else if (arg == "--no-quotes") {
            quotes = false;
        } else if (arg.find("--citesize=") == 0) {
            citeSizeOvr = stoi(arg.substr(11));
        } else if (arg.find("--citescale=") == 0) {
            citeScalePct = stoi(arg.substr(12));
        } else if (arg.find("--citestyle=") == 0) {
            citeStyle = arg.substr(12);
        } else if (arg.find("--citeplacement=") == 0) {
            citePlacement = arg.substr(16);
        } else if (arg.find("--citebibleversion=") == 0) {
            string val = arg.substr(19);
            citeBibleVersion = (val != "no" && val != "false");
        } else if (arg.find("--citeshadow=") == 0) {
            citeShadow = max(0, min(10, stoi(arg.substr(13))));
        } else if (arg == "--citeshadow") {
            citeShadow = 5;
        } else if (arg == "--no-citeshadow") {
            citeShadow = 0;
        } else if (arg.find("--citealign=") == 0) {
            citeAlign = arg.substr(12);
        } else if (arg.find("--citepanel=") == 0) {
            citePanel = arg.substr(12);
        } else if (arg.find("--textsize=") == 0) {
            textSizePt = stoi(arg.substr(11));
        } else if (arg.find("--maxtextsize=") == 0) {
            maxTextSizePt = stoi(arg.substr(14));
        } else if (arg.find("--textscale=") == 0) {
            textScalePct = stoi(arg.substr(12));
        } else if (arg.find("--textpanel=") == 0) {
            textPanelOpacity = stoi(arg.substr(12));
        } else if (arg.find("--textpanelcolor=") == 0) {
            textPanelColor = arg.substr(17);
        } else if (arg.find("--textshadow=") == 0) {
            textShadow = max(0, min(10, stoi(arg.substr(13))));
        } else if (arg == "--textshadow") {
            textShadow = 5;
        } else if (arg == "--no-textshadow") {
            textShadow = 0;
        } else if (arg.find("--shadowmethod=") == 0) {
            shadowMethod = max(1, min(2, stoi(arg.substr(15))));
        } else if (arg.find("--textoutline=") == 0) {
            textOutline = max(0, stoi(arg.substr(14)));
        } else if (arg == "--textoutline") {
            textOutline = 2;
        } else if (arg == "--no-textoutline") {
            textOutline = 0;
        } else if (arg.find("--textoutlinecolor=") == 0) {
            textOutlineColor = arg.substr(19);
        } else if (arg.find("--linespacing=") == 0) {
            lineSpacing = stoi(arg.substr(14));
        } else if (arg.find("--textoffy=") == 0) {
            textOffsetY = stoi(arg.substr(11));
        } else if (arg.find("--citeoffy=") == 0) {
            citeOffsetY = stoi(arg.substr(11));
        } else if (arg.find("--reserve=") == 0) {
            string val = arg.substr(10);
            size_t comma = val.find(',');
            if (comma != string::npos) {
                string side = val.substr(0, comma);
                int pct = max(0, min(90, stoi(val.substr(comma + 1))));
                if      (side == "top")    reserveTop    = pct;
                else if (side == "right")  reserveRight  = pct;
                else if (side == "bottom") reserveBottom = pct;
                else if (side == "left")   reserveLeft   = pct;
                else { cerr << "Error: --reserve= side must be top, right, bottom, or left.\n"; return 1; }
            }
        } else if (arg == "--textpanelrounded") {
            panelRounded = true;
        } else if (arg == "--no-textpanelrounded") {
            panelRounded = false;
        } else if (arg.find("--ref=") == 0) {
            reference = arg.substr(6);
        } else if (arg.find("-ref=") == 0) {
            reference = arg.substr(5);
        } else if (arg.find("--text=") == 0) {
            customText = arg.substr(7);
        } else if (arg == "--saveconfig") {
            saveConfig = true;
        } else if (arg == "--showconfig") {
            showConfig = true;
        } else if (arg.find("-") == 0) {
            cerr << "Error: unknown option '" << arg << "'\n";
            cerr << "Run 'bvi --help' for usage.\n";
            return 1;
        } else if (reference.empty()) {
            reference = arg;
        } else {
            cerr << "Error: unexpected argument '" << arg << "'\n";
            cerr << "Run 'bvi --help' for usage.\n";
            return 1;
        }
    }

#ifdef _WIN32
    // ImageMagick font lookup treats '\' as escape sequences; normalize to '/'.
    for (char& c : font)     if (c == '\\') c = '/';
    for (char& c : citeFont) if (c == '\\') c = '/';
#endif

    if ((textSizePt > 0 || maxTextSizePt > 0) && textScalePct != 100) {
        cerr << "Error: --textsize/--maxtextsize and --textscale cannot be used together.\n";
        return 1;
    }
    if (citeSizeOvr > 0 && citeScalePct != 100) {
        cerr << "Error: --citesize and --citescale cannot be used together.\n";
        return 1;
    }
    if (citeStyle != "dash" && citeStyle != "parens" && citeStyle != "plain" && citeStyle != "none") {
        cerr << "Error: --citestyle must be dash, parens, or plain.\n";
        return 1;
    }
    if (citePlacement != "bottom" && citePlacement != "below") {
        cerr << "Error: --citeplacement must be bottom or below.\n";
        return 1;
    }
    if (citeAlign != "center" && citeAlign != "left" && citeAlign != "right") {
        cerr << "Error: --citealign must be center, left, or right.\n";
        return 1;
    }
    if (citePanel != "independent" && citePanel != "coverbottom" && citePanel != "none") {
        cerr << "Error: --citepanel must be independent, coverbottom, or none.\n";
        return 1;
    }

    // ── --showconfig: print effective settings and exit ───────────────────
    if (showConfig) {
        cout << "Effective settings (config file + command-line):\n";
        cout << "  bv               = " << version   << "\n";
        cout << "  width            = " << imgWidth  << "\n";
        cout << "  height           = " << imgHeight << "\n";
        cout << "  font             = " << font      << "\n";
        cout << "  bg               = " << bgColor   << "\n";
        cout << "  bgphoto          = " << (bgPhoto.empty() ? "(none)" : bgPhoto) << "\n";
        cout << "  dim              = " << dimPct    << "\n";
        cout << "  textcolor        = " << textColor << "\n";
        cout << "  citecolor        = " << citeColor << "\n";
        cout << "  citefont         = " << (citeFont.empty() ? "(same as font)" : citeFont) << "\n";
        cout << "  quotes           = " << (quotes ? "yes" : "no") << "\n";
        cout << "  citesize         = " << (citeSizeOvr > 0 ? to_string(citeSizeOvr) : "auto") << "\n";
        cout << "  citescale        = " << citeScalePct << "%\n";
        cout << "  citestyle        = " << citeStyle     << "\n";
        cout << "  citeplacement    = " << citePlacement << "\n";
        cout << "  citebibleversion = " << (citeBibleVersion ? "yes" : "no") << "\n";
        cout << "  citeshadow       = " << (citeShadow > 0 ? to_string(citeShadow) : "no") << "\n";
        cout << "  citealign        = " << citeAlign << "\n";
        cout << "  citepanel        = " << citePanel << "\n";
        cout << "  textsize         = " << (textSizePt    > 0 ? to_string(textSizePt)    : "off") << "\n";
        cout << "  maxtextsize      = " << (maxTextSizePt > 0 ? to_string(maxTextSizePt) : "off") << "\n";
        cout << "  textscale        = " << textScalePct << "%\n";
        cout << "  textpanel        = " << (textPanelOpacity > 0 ? to_string(textPanelOpacity) + "%" : "off") << "\n";
        cout << "  textpanelcolor   = " << textPanelColor << "\n";
        cout << "  textpanelrounded = " << (panelRounded ? "yes" : "no") << "\n";
        cout << "  textshadow       = " << (textShadow > 0 ? to_string(textShadow) : "no") << "\n";
        cout << "  shadowmethod     = " << shadowMethod << "\n";
        cout << "  textoutline      = " << (textOutline > 0 ? to_string(textOutline) : "no") << "\n";
        cout << "  textoutlinecolor = " << textOutlineColor << "\n";
        cout << "  linespacing      = " << lineSpacing << "\n";
        cout << "  textoffy         = " << textOffsetY << "\n";
        cout << "  citeoffy         = " << citeOffsetY << "\n";
        cout << "  reservetop       = " << reserveTop    << "\n";
        cout << "  reserveright     = " << reserveRight  << "\n";
        cout << "  reservebottom    = " << reserveBottom << "\n";
        cout << "  reserveleft      = " << reserveLeft   << "\n";
        ifstream check(CONFIG_FILE);
        if (check.good())
            cout << "\nConfig file: ./" << CONFIG_FILE << " (loaded)\n";
        else {
            const char* home = getenv(HOME_ENV);
            string homeCfg = home ? string(home) + "/" + CONFIG_FILE : "";
            ifstream homeCheck(homeCfg);
            if (!homeCfg.empty() && homeCheck.good())
                cout << "\nConfig file: " << homeCfg << " (loaded)\n";
            else
                cout << "\nConfig file: " << CONFIG_FILE << " (not found — using defaults)\n";
        }
        return 0;
    }

    // ── --saveconfig: write [bvi] section of .verselumen and exit ────────
    if (saveConfig) {
        vector<string> lines = {
            "bv               = " + version,
            "width            = " + to_string(imgWidth),
            "height           = " + to_string(imgHeight),
            "font             = " + font,
            "bg               = " + bgColor,
            "bgphoto          = " + bgPhoto,
            "dim              = " + to_string(dimPct),
            "textcolor        = " + textColor,
            "citecolor        = " + citeColor,
            "citefont         = " + citeFont,
            "quotes           = " + string(quotes ? "yes" : "no"),
            "citesize         = " + to_string(citeSizeOvr),
            "citescale        = " + to_string(citeScalePct),
            "citestyle        = " + citeStyle,
            "citeplacement    = " + citePlacement,
            "citebibleversion = " + string(citeBibleVersion ? "yes" : "no"),
            "citeshadow       = " + (citeShadow > 0 ? to_string(citeShadow) : string("no")),
            "citealign        = " + citeAlign,
            "citepanel        = " + citePanel,
            "textsize         = " + to_string(textSizePt),
            "maxtextsize      = " + to_string(maxTextSizePt),
            "textscale        = " + to_string(textScalePct),
            "textpanel        = " + to_string(textPanelOpacity),
            "textpanelcolor   = " + textPanelColor,
            "textpanelrounded = " + string(panelRounded ? "yes" : "no"),
            "textshadow       = " + (textShadow > 0 ? to_string(textShadow) : string("no")),
            "shadowmethod     = " + to_string(shadowMethod),
            "textoutline      = " + (textOutline > 0 ? to_string(textOutline) : string("0")),
            "textoutlinecolor = " + textOutlineColor,
            "linespacing      = " + to_string(lineSpacing),
            "textoffy         = " + to_string(textOffsetY),
            "citeoffy         = " + to_string(citeOffsetY),
            "reservetop       = " + to_string(reserveTop),
            "reserveright     = " + to_string(reserveRight),
            "reservebottom    = " + to_string(reserveBottom),
            "reserveleft      = " + to_string(reserveLeft)
        };
        if (!writeSection(lines)) { cerr << "Error: could not write '" << CONFIG_FILE << "'.\n"; return 1; }
        cerr << "Saved [" << SECTION << "] to ./" << CONFIG_FILE << "\n";
        return 0;
    }

    if (reference.empty() && customText.empty()) {
        cerr << "Error: no Bible reference or text given.\n";
        cerr << "Usage: bvi \"Reference\" [OPTIONS]\n";
        cerr << "       bvi --text=\"Custom text\" [OPTIONS]\n";
        cerr << "Example: bvi \"John 3:16\"\n";
        return 1;
    }

    if (outputFile.empty())
        outputFile = reference.empty() ? "bvi_output.jpg" : refToFilename(reference);

    // ── Verse text ────────────────────────────────────────────────────────
    string verseText;
    if (!customText.empty()) {
        verseText = customText;
        if (reference.empty())
            citeStyle = "none";
    } else {
        // ── Bible version / file resolution ──────────────────────────────
        transform(version.begin(), version.end(), version.begin(), ::toupper);

        string bibleFile, bibleUrl;
        if (version == "KJV") {
            bibleFile = "BibleKJV.txt";
            bibleUrl  = "https://raw.githubusercontent.com/codelife-us/VerseLumen/main/BibleKJV.txt";
        } else if (version == "BSB") {
            bibleFile = "BibleBSB.txt";
            bibleUrl  = "https://bereanbible.com/bsb.txt";
        } else if (version == "WEB") {
            bibleFile = "BibleWEB.txt";
            bibleUrl  = "https://raw.githubusercontent.com/codelife-us/VerseLumen/main/BibleWEB.txt";
        } else {
            cerr << "Error: unsupported Bible version '" << version << "'.\n";
            cerr << "Supported versions: KJV, BSB, WEB\n";
            return 1;
        }

        // Check Bible file; try HOME directory fallback, then offer to download
        {
            ifstream test(bibleFile);
            if (!test.good()) {
                const char* home = getenv(HOME_ENV);
                if (home) {
                    string homePath = string(home) + "/" + bibleFile;
                    ifstream homeTest(homePath);
                    if (homeTest.good()) {
                        bibleFile = homePath;
                        goto bible_ready;
                    }
                }
                cerr << "Bible file '" << bibleFile << "' not found.\n";
                cerr << "Download it now? (y/n): ";
                char answer;
                cin >> answer;
                if (answer == 'y' || answer == 'Y') {
                    string tmpFile = bibleFile + ".tmp";
                    string cmd = "curl -L --fail \"" + bibleUrl + "\" -o \"" + tmpFile + "\"";
                    if (runSystem(cmd) != 0) {
                        remove(tmpFile.c_str());
                        cerr << "Download failed. Please download manually:\n  " << bibleUrl << "\n";
                        return 1;
                    }
                    {
                        ifstream chk(tmpFile);
                        string first;
                        getline(chk, first);
                        if (first.find('<') != string::npos) {
                            remove(tmpFile.c_str());
                            cerr << "Download returned HTML instead of Bible text.\nPlease download manually:\n  " << bibleUrl << "\n";
                            return 1;
                        }
                    }
                    if (rename(tmpFile.c_str(), bibleFile.c_str()) != 0) {
                        remove(tmpFile.c_str());
                        cerr << "Failed to save " << bibleFile << "\n";
                        return 1;
                    }
                    cout << "Downloaded " << bibleFile << " successfully.\n";
                } else {
                    cerr << "Cannot continue without Bible file. Exiting.\n";
                    return 1;
                }
            }
        }
        bible_ready:

        bibleVerses = loadBible(bibleFile);

        verseText = lookupVerses(reference);
        if (verseText.empty()) {
            cerr << "Error: reference not found: " << reference << "\n";
            return 1;
        }
    }

    // Optionally wrap verse in curly quotation marks
    if (quotes)
        verseText = "\xe2\x80\x9c" + verseText + "\xe2\x80\x9d";

    // Citation line — format depends on --citestyle and --citebibleversion
    string citation;
    string ver = citeBibleVersion ? " (" + version + ")" : "";
    if (citeStyle == "parens")
        citation = citeBibleVersion ? "(" + reference + ", " + version + ")" : "(" + reference + ")";
    else if (citeStyle == "plain")
        citation = reference + ver;
    else // dash (default)
        citation = "\xe2\x80\x94 " + reference + ver;

    string quotedVerse    = shellQuote(verseText);
    string quotedCitation = shellQuote(citation);

    // ── Layout calculations ───────────────────────────────────────────────
    // Scale all measurements proportionally to the chosen image size.
    double scale = (double)imgHeight / 1080.0;

    // Verse text area: ~100px margin each side, and enough height to let
    // caption: auto-fit the font. The layer is trimmed after generation so
    // the actual text block (not the full canvas) is centered on the image.
    int verseW    = (int)(imgWidth  * 0.896 * textScalePct / 100.0);   // ~1720px at 1920 wide
    int verseH    = (int)(imgHeight * 0.741 * textScalePct / 100.0);  // ~800px at 1080 tall — caption auto-fits within this
    // Shift the trimmed verse block slightly above center so the citation fits below.
    int verseOffY = (citeStyle == "none") ? 0 : (int)(40 * scale);  // pixels above image center

    // Citation: fixed point size, placed near the bottom edge.
    int citePt    = (citeSizeOvr > 0) ? citeSizeOvr : max(1, (int)(60 * scale * citeScalePct / 100.0)); // ~60pt at 1080p
    int citeOffY  = max(0, max(20, (int)(55 * scale)) - citeOffsetY); // pixels inward from bottom edge
    verseOffY    -= textOffsetY;   // positive textOffsetY moves verse down

    // Reserve: shrink canvas into remaining area and shift center.
    double fracTop    = reserveTop    / 100.0;
    double fracRight  = reserveRight  / 100.0;
    double fracBottom = reserveBottom / 100.0;
    double fracLeft   = reserveLeft   / 100.0;
    int reserveShiftX = (int)(imgWidth  * (fracLeft - fracRight) / 2.0);
    int reserveShiftY = (int)(imgHeight * (fracTop  - fracBottom) / 2.0);
    verseW = max(1, (int)(verseW * (1.0 - fracLeft - fracRight)));
    verseH = max(1, (int)(verseH * (1.0 - fracTop  - fracBottom)));
    verseOffY -= reserveShiftY;  // bvi verseOffY is above-center; top reserve shifts center down → decrease
    string rGeomX = (reserveShiftX >= 0 ? "+" : "") + to_string(reserveShiftX);

    int citeMarginX = (imgWidth - (int)(imgWidth * 0.896)) / 2; // matches verse left/right margin

    // Gravity strings based on citealign (center/left/right) and placement (below/bottom).
    // "below" uses Center/West/East; "bottom" uses South/SouthWest/SouthEast.
    string citeGravityCenter = (citePlacement == "below") ? "Center" : "South";
    string citeGravityLeft   = (citePlacement == "below") ? "West"   : "SouthWest";
    string citeGravityRight  = (citePlacement == "below") ? "East"   : "SouthEast";
    string citeGravity = (citeAlign == "left") ? citeGravityLeft
                       : (citeAlign == "right") ? citeGravityRight
                       : citeGravityCenter;
    int citeX = (citeAlign == "center") ? 0 : citeMarginX;

    // Temp file for the intermediate verse layer PNG.
    string tmpLayer = outputFile + ".tmp_layer.png";

    // ── Step 1: Render verse text to a trimmed PNG layer ──────────────────
    //
    // caption:@file auto-sizes font to fill the given canvas.
    // -trim removes excess background so the layer height matches the text,
    // allowing gravity Center to truly center the text block on the canvas.
    //
    // Note: -size must be set here, not inherited from a prior xc: command,
    // to avoid the global -size leaking into the caption: dimensions.
    int borderH = max(10, (int)(20 * scale));
    int borderW = max(20, (int)(40 * scale));

    if (im.empty()) {
        cerr << "Error: ImageMagick not found. Install it with:\n";
        cerr << "  macOS:   brew install imagemagick\n";
        cerr << "  Linux:   apt install imagemagick\n";
        cerr << "  Windows: choco install imagemagick\n";
        return 1;
    }

    // When using a photo background or text shadow the verse layer needs a
    // transparent background so it composites cleanly.
    string layerBg = (bgPhoto.empty() && textShadow == 0 && textOutline == 0) ? bgColor : "none";

    // Determine whether and at what size to pass -pointsize to caption:.
    // caption: with -pointsize uses a fixed/max size rather than auto-filling,
    // so for maxtextsize we only clamp when the auto-fit size would exceed the cap.
    bool applyPointsize  = false;
    int  applyPointsizeN = 0;
    if (maxTextSizePt > 0) {
        // Cap: query auto-fit first, apply -pointsize only if it would exceed the cap.
        ostringstream qcmd;
        qcmd << im
             << " -background \"" << layerBg << "\""
             << " -fill \""       << textColor << "\""
             << " -font \""       << font << "\""
             << " -gravity Center"
             << (lineSpacing != 0 ? " -interline-spacing " + to_string(lineSpacing) : "")
             << " -size "         << verseW << "x" << verseH
             << " caption:"       << quotedVerse
             << " -verbose info:";
        int autoFitSize = 0;
        FILE* pipe = runPopen(qcmd.str(), "r");
        if (pipe) {
            char buf[256];
            while (fgets(buf, sizeof(buf), pipe)) {
                if (sscanf(buf, " caption:pointsize: %d", &autoFitSize) == 1)
                    break;
            }
            pclose(pipe);
        }
        applyPointsize  = (autoFitSize == 0 || autoFitSize > maxTextSizePt);
        applyPointsizeN = maxTextSizePt;
    } else if (textSizePt > 0) {
        // Absolute: always render at exactly the requested point size; no query needed.
        applyPointsize  = true;
        applyPointsizeN = textSizePt;
    }

    // When a fixed pointsize is applied, use a tall canvas so caption: renders all
    // the text instead of clipping it to verseH. The actual layer is trimmed after,
    // and clamping below keeps the result within the canvas.
    int captionH = applyPointsize ? max(verseH, imgHeight * 4) : verseH;

    ostringstream cmd1;
    cmd1 << im
         << " -background \"" << layerBg << "\""
         << " -fill \""       << textColor << "\""
         << " -font \""       << font << "\""
         << " -gravity Center"                        // center-align each text line
         << (lineSpacing != 0 ? " -interline-spacing " + to_string(lineSpacing) : "")
         << " -size "         << verseW << "x" << captionH
         << (applyPointsize ? " -pointsize " + to_string(applyPointsizeN) : "")
         << " caption:"      << quotedVerse
         << " -trim"                                  // crop to actual text bounds
         << " -bordercolor \"" << layerBg << "\""     // restore padding around text
         << " -border " << borderW << "x" << borderH
         << " \"" << tmpLayer << "\"";

    int ret1 = runSystem(cmd1.str());
    if (ret1 != 0) {
        cerr << "Error: verse layer generation failed.\n\n";
        cerr << "To list fonts or use a font by path:\n";
        cerr << "  magick -list font\n";
        cerr << "  bvi \"...\" --font=\"/path/to/font.ttf\"\n";
        return 1;
    }

    // ── Optional outline step (before shadow so shadow uses outlined shape) ─
    string tmpOutline = outputFile + ".tmp_outline.png";
    string tmpShadow  = outputFile + ".tmp_shadow.png";
    string activeLayer = tmpLayer;
    if (textOutline > 0) {
        ostringstream outlineCmd;
        outlineCmd << im << " \"" << activeLayer << "\""
                   << " " << LP << " +clone -channel alpha"
                   << " -morphology Dilate disk:" << textOutline
                   << " +channel -fill \"" << textOutlineColor << "\" -colorize 100 " << RP
                   << " +swap -composite"
                   << " \"" << tmpOutline << "\"";
        if (runSystem(outlineCmd.str()) == 0)
            activeLayer = tmpOutline;
    }

    // ── Optional shadow step ──────────────────────────────────────────────
    if (textShadow > 0) {
        double sigma   = (shadowMethod == 1) ? textShadow * 0.8 : 0.0;
        int    offset  = max(1, (int)round(textShadow * 0.6));
        int    opacity = (shadowMethod == 1) ? 80 : 100;
        ostringstream shadowCmd;
        shadowCmd << im << " \"" << activeLayer << "\""
                  << " " << LP << " +clone -background black -shadow " << opacity << "x" << sigma
                  << "+" << offset << "+" << offset << " " << RP
                  << " +swap -background none -flatten"
                  << " \"" << tmpShadow << "\"";
        if (runSystem(shadowCmd.str()) == 0)
            activeLayer = tmpShadow;
    }

    // ── Query layer dimensions ────────────────────────────────────────────
    int layerW = 0, layerH = 0;
    {
        string identCmd = im + " identify -format \"%wx%h\" \"" + tmpLayer + "\"";
        FILE* pipe = runPopen(identCmd, "r");
        if (pipe) {
            char buf[64] = {};
            fgets(buf, sizeof(buf), pipe);
            pclose(pipe);
            sscanf(buf, "%dx%d", &layerW, &layerH);
        }
    }

    // Clamp verseOffY so the text layer stays within the canvas bounds.
    if (layerH > 0) {
        int halfH = layerH / 2;
        if (layerH >= imgHeight) {
            // Layer taller than canvas: anchor its top to the canvas top so the
            // beginning of the verse is visible (overflow falls off the bottom).
            verseOffY = imgHeight / 2 - halfH;
        } else {
            verseOffY = max(halfH - imgHeight / 2, min(imgHeight / 2 - halfH, verseOffY));
        }
    }

    // ── Panel fragment ────────────────────────────────────────────────────
    ostringstream panelDraw;
    if (textPanelOpacity > 0 && layerW > 0 && layerH > 0) {
        int panelH    = layerH;
        int panelOffY = verseOffY;   // pixels above canvas center (positive = above)

        if (citeStyle != "none" && citePlacement == "below") {
            // Extend the panel down to enclose the citation.
            int visibleGap = max(8,  (int)(16 * scale));
            int bottomPad  = max(12, (int)(28 * scale));
            int extraH     = visibleGap + citePt + bottomPad;
            panelH   += extraH;
            panelOffY = verseOffY - extraH / 2;  // may go negative (below canvas center)
        } else if (citeStyle != "none" && citePlacement == "bottom" && citePanel == "coverbottom") {
            // Stretch the main panel downward so it reaches the citation near the bottom edge.
            // Keep the panel's top edge fixed; extend the bottom to cover the citation.
            int citePad     = max(6, (int)(14 * scale));
            int panelTopY   = imgHeight / 2 - panelOffY - panelH / 2;   // from image top
            int citeBotY    = imgHeight - citeOffY + citePad;             // citation bottom from image top
            int newPanelH   = max(panelH, citeBotY - panelTopY);
            panelOffY       = imgHeight / 2 - (panelTopY + newPanelH / 2);
            panelH          = newPanelH;
        }

        int cornerR = max(4, (int)(12 * scale));
        if (panelRounded) {
            panelDraw << " " << LP << " -size " << layerW << "x" << panelH
                      << " xc:none -fill \"" << textPanelColor << "\""
                      << " -draw \"roundrectangle 0,0 " << (layerW-1) << "," << (panelH-1)
                      << " " << cornerR << "," << cornerR << "\""
                      << " -alpha set -channel Alpha -evaluate multiply "
                      << (textPanelOpacity / 100.0) << " +channel " << RP;
        } else {
            panelDraw << " " << LP << " -size " << layerW << "x" << panelH
                      << " xc:\"" << textPanelColor << "\""
                      << " -alpha set -channel Alpha -evaluate multiply "
                      << (textPanelOpacity / 100.0) << " +channel " << RP;
        }
        panelDraw << " -gravity Center"
                  << " -geometry " << rGeomX << (panelOffY >= 0 ? "-" : "+") << abs(panelOffY)
                  << " -composite";

        // Independent mode: separate narrow panel behind the bottom citation.
        if (citeStyle != "none" && citePlacement == "bottom" && citePanel == "independent") {
            int citePad = max(6, (int)(14 * scale));
            string activeCiteFontI = citeFont.empty() ? font : citeFont;

            // Measure actual rendered text bounds so the panel aligns to visible pixels,
            // not to the annotation baseline (which varies widely by font).
            int textBotFB = citeOffY;            // fallback: annotation offset ≈ text bottom
            int textTopFB = citeOffY + citePt;   // fallback: assume text fills citePt height
            {
                ostringstream meas;
                meas << im
                     << " -size " << imgWidth << "x" << imgHeight
                     << " xc:none -fill white"
                     << " -font \"" << activeCiteFontI << "\""
                     << " -pointsize " << citePt
                     << " -gravity " << citeGravity
                     << " -annotate +" << citeX << "+" << citeOffY
                     << " " << quotedCitation
                     << " -trim"
                     << " -format \"%[fx:" << imgHeight << "-page.y-h] %[fx:" << imgHeight << "-page.y]\""
                     << " info:";
                FILE* pipe = runPopen(meas.str(), "r");
                if (pipe) {
                    int b = 0, t = 0;
                    if (fscanf(pipe, "%d %d", &b, &t) == 2 && t > b && b >= 0)
                    { textBotFB = b; textTopFB = t; }
                    pclose(pipe);
                }
            }
            int citePanelH   = (textTopFB - textBotFB) + 2 * citePad;
            int citePanelOff = max(0, textBotFB - citePad);

            if (panelRounded) {
                panelDraw << " " << LP << " -size " << layerW << "x" << citePanelH
                          << " xc:none -fill \"" << textPanelColor << "\""
                          << " -draw \"roundrectangle 0,0 " << (layerW-1) << "," << (citePanelH-1)
                          << " " << cornerR << "," << cornerR << "\""
                          << " -alpha set -channel Alpha -evaluate multiply "
                          << (textPanelOpacity / 100.0) << " +channel " << RP;
            } else {
                panelDraw << " " << LP << " -size " << layerW << "x" << citePanelH
                          << " xc:\"" << textPanelColor << "\""
                          << " -alpha set -channel Alpha -evaluate multiply "
                          << (textPanelOpacity / 100.0) << " +channel " << RP;
            }
            panelDraw << " -gravity South -geometry +0+" << citePanelOff
                      << " -composite";
        }
    }

    // ── Citation annotation fragment ──────────────────────────────────────
    ostringstream citeAnnot;
    if (citeStyle != "none") {
        string activeCiteFont = citeFont.empty() ? font : citeFont;
        double citeSigma  = citeShadow * 0.8 * scale;
        int    shadowOff  = max(1, (int)round(3 * scale * citeShadow / 5.0));

        if (citePlacement == "below") {
            int visibleGap = max(8, (int)(16 * scale));
            int offset     = -verseOffY + layerH / 2 + visibleGap + citePt * 3 / 4 + citeOffsetY;

            if (citeShadow > 0) {
                int shadowY = offset + shadowOff;
                if (shadowMethod == 1) {
                    citeAnnot << " " << LP << " -size " << imgWidth << "x" << imgHeight << " xc:none"
                              << " -fill black"
                              << " -font \"" << activeCiteFont << "\""
                              << " -pointsize " << citePt
                              << " -gravity " << citeGravity
                              << " -annotate +" << (citeX + shadowOff)
                              << (shadowY >= 0 ? "+" : "-") << abs(shadowY)
                              << " " << quotedCitation
                              << " -blur 0x" << citeSigma
                              << " -channel Alpha -evaluate multiply 0.8 +channel " << RP
                              << " -composite";
                } else {
                    citeAnnot << " -fill black"
                              << " -font \"" << activeCiteFont << "\""
                              << " -pointsize " << citePt
                              << " -gravity " << citeGravity
                              << " -annotate +" << (citeX + shadowOff)
                              << (shadowY >= 0 ? "+" : "-") << abs(shadowY)
                              << " " << quotedCitation;
                }
            }

            citeAnnot << " -fill \"" << citeColor << "\""
                      << " -font \"" << activeCiteFont << "\""
                      << " -pointsize " << citePt
                      << " -gravity " << citeGravity
                      << (offset >= 0 ? " -annotate +" : " -annotate +") << citeX
                      << (offset >= 0 ? "+" : "-") << abs(offset)
                      << " " << quotedCitation;
        } else {
            if (citeShadow > 0) {
                int shadowSouthY = max(0, citeOffY - shadowOff);
                if (shadowMethod == 1) {
                    citeAnnot << " " << LP << " -size " << imgWidth << "x" << imgHeight << " xc:none"
                              << " -fill black"
                              << " -font \"" << activeCiteFont << "\""
                              << " -pointsize " << citePt
                              << " -gravity " << citeGravity
                              << " -annotate +" << (citeX + shadowOff) << "+" << shadowSouthY
                              << " " << quotedCitation
                              << " -blur 0x" << citeSigma
                              << " -channel Alpha -evaluate multiply 0.8 +channel " << RP
                              << " -composite";
                } else {
                    citeAnnot << " -fill black"
                              << " -font \"" << activeCiteFont << "\""
                              << " -pointsize " << citePt
                              << " -gravity " << citeGravity
                              << " -annotate +" << (citeX + shadowOff) << "+" << shadowSouthY
                              << " " << quotedCitation;
                }
            }

            citeAnnot << " -fill \"" << citeColor << "\""
                      << " -font \"" << activeCiteFont << "\""
                      << " -pointsize " << citePt
                      << " -gravity " << citeGravity
                      << " -annotate +" << citeX << "+" << citeOffY
                      << " " << quotedCitation;
        }
    }

    // ── Step 2: Composite verse layer + annotate citation ─────────────────
    //
    // The trimmed verse layer is centered on the canvas (shifted slightly
    // above center to leave room for the citation at the bottom, or placed
    // inline when --citeplacement=below).
    ostringstream cmd2;
    if (bgPhoto.empty()) {
        cmd2 << im
             << " -size " << imgWidth << "x" << imgHeight
             << " xc:\"" << bgColor << "\""
             << panelDraw.str()
             << " \"" << activeLayer << "\""
             << " -gravity Center"
             << " -geometry " << rGeomX << (verseOffY >= 0 ? "-" : "+") << abs(verseOffY)
             << " -composite"
             << citeAnnot.str()
             << " \"" << outputFile << "\"";
    } else {
        // Load photo, resize to fill canvas, crop to exact size, then dim.
        int clampedDim = max(0, min(100, dimPct));
        cmd2 << im
             << " \"" << bgPhoto << "\""
             << " -resize " << imgWidth << "x" << imgHeight << "^"
             << " -gravity Center"
             << " -extent " << imgWidth << "x" << imgHeight
             << " -fill black -colorize " << clampedDim
             << panelDraw.str()
             << " \"" << activeLayer << "\""
             << " -gravity Center"
             << " -geometry " << rGeomX << (verseOffY >= 0 ? "-" : "+") << abs(verseOffY)
             << " -composite"
             << citeAnnot.str()
             << " \"" << outputFile << "\"";
    }

    int ret2 = runSystem(cmd2.str());

    remove(tmpLayer.c_str());
    if (textOutline > 0) remove(tmpOutline.c_str());
    if (textShadow > 0) remove(tmpShadow.c_str());

    if (ret2 != 0) {
        cerr << "Error: image generation failed.\n\n";
        cerr << "To list fonts or use a font by path:\n";
        cerr << "  magick -list font\n";
        cerr << "  bvi \"...\" --font=\"/path/to/font.ttf\"\n";
        return 1;
    }

    cerr << "Saved to " << outputFile << "\n";
    return 0;
}
