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
// textimage.cpp  (Text Image)
// Renders plain text to a JPEG image with auto-fitted text.
// Requires ImageMagick (magick / convert).

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

const string TI_VERSION  = "1.0";
const string CONFIG_FILE = ".verselumen";
const string SECTION     = "textimage";

// ── Config file (.verselumen, [textimage] section) ───────────────────────────

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

string cfgGet(const map<string, string>& cfg, const string& key, const string& defaultVal) {
    auto it = cfg.find(key);
    return (it != cfg.end()) ? it->second : defaultVal;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

// Build a safe default filename from the first few words of the text.
string textToFilename(const string& text) {
    string s;
    int wordCount = 0;
    bool inWord = false;
    for (char c : text) {
        if (c == ' ' || c == '\t' || c == '\n') {
            if (inWord) { ++wordCount; inWord = false; if (wordCount >= 4) break; s += '_'; }
        } else if (isalnum((unsigned char)c) || c == '\'' || c == '-') {
            s += c;
            inWord = true;
        }
    }
    if (s.empty() || s.back() == '_') {
        while (!s.empty() && s.back() == '_') s.pop_back();
        if (s.empty()) s = "text_image";
    }
    return s + ".jpg";
}

string shellQuote(const string& s) {
#ifdef _WIN32
    string r = "\"";
    for (char c : s) {
        if (c == '"') r += "\"\"";
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

string detectIM() {
#ifdef _WIN32
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
    cout << "textimage v" << TI_VERSION << "\n\n";
    cout << "Usage: textimage \"Text to render\" [OPTIONS]\n";
    cout << "       textimage --text=\"Text to render\" [OPTIONS]\n\n";
    cout << "Options:\n";
    cout << "  -h, --help              Show this help message and exit\n";
    cout << "  --text=TEXT             Text to render (alternative to positional argument)\n";
    cout << "  --text2=TEXT            Second text block stacked below the first\n";
    cout << "  --text2gap=N            Pixel gap between the two text blocks (default: 40)\n";
    cout << "  --text2color=COLOR      Text color for the second block (default: same as --textcolor)\n";
    cout << "  --text2font=FONT        Font for the second block (default: same as --font)\n";
    cout << "  --text2outline[=N]      Outline px for text2 (default: same as --textoutline)\n";
    cout << "  --no-text2outline       No outline for text2\n";
    cout << "  --text2outlinecolor=C   Outline color for text2 (default: same as --textoutlinecolor)\n";
    cout << "  --text2shadow[=N]       Shadow intensity for text2 (default: same as --textshadow)\n";
    cout << "  --no-text2shadow        No shadow for text2\n";
    cout << "  --text2shadowmethod=N   Shadow method for text2 (default: same as --shadowmethod)\n";
    cout << "  --output=FILE           Output image file (default: derived from text)\n";
    cout << "  --width=N               Image width in pixels (default: 1920)\n";
    cout << "  --height=N              Image height in pixels (default: 1080)\n";
    cout << "  --font=FONT             Font name or file path\n";
    cout << "  --bg=COLOR              Background color (default: black)\n";
    cout << "  --bgphoto=FILE          Background photo (jpg/png); overrides --bg\n";
    cout << "  --dim=N                 Darken photo 0-100% (default: 50); ignored without --bgphoto\n";
    cout << "  --textcolor=COLOR       Text color (default: white)\n";
    cout << "  --textsize=N            Force font to exactly N points (cannot combine with --textscale)\n";
    cout << "  --maxtextsize=N         Cap auto-fit font at N points; overrides --textsize\n";
    cout << "  --textscale=PCT         Scale text area to PCT% of default (e.g. 75); cannot combine with --textsize/--maxtextsize\n";
    cout << "  --textpanel=N           Semi-transparent panel behind text, N=opacity 1-100 (default: off)\n";
    cout << "  --textpanelcolor=COLOR  Panel color (default: black); any ImageMagick color\n";
    cout << "  --textpanelrounded      Rounded corners on text panel\n";
    cout << "  --no-textpanelrounded   Square corners (default)\n";
    cout << "  --textshadow[=N]        Add drop shadow behind text; N=1-10 intensity (default 5)\n";
    cout << "  --no-textshadow         Remove drop shadow (default)\n";
    cout << "  --shadowmethod=N        Shadow style: 1=soft Gaussian blur (default), 2=hard offset copy\n";
    cout << "  --textoutline[=N]       Outline width in pixels around text (default 2)\n";
    cout << "  --no-textoutline        Remove outline (default)\n";
    cout << "  --textoutlinecolor=C    Outline color (default: black); any ImageMagick color\n";
    cout << "  --linespacing=N         Adjust line spacing: positive=more, negative=less, 0=default\n";
    cout << "  --textoffy=N            Shift text vertically; positive=down, negative=up (default 0)\n";
    cout << "  --reserve=SIDE,PCT      Reserve PCT% (0-90) from SIDE (top/right/bottom/left);\n";
    cout << "                          repeat for multiple sides (e.g. --reserve=left,30 --reserve=top,20)\n\n";
    cout << "Config file (.verselumen in current directory or $HOME, [textimage] section):\n";
    cout << "  --saveconfig            Save current settings to .verselumen [textimage] as new defaults\n";
    cout << "  --showconfig            Print current effective settings and exit\n\n";
    cout << "  Supported keys in [textimage]:  width  height  font  bg  bgphoto  dim  textcolor  textsize  maxtextsize  textscale  textpanel  textpanelcolor  textpanelrounded  textshadow  shadowmethod  textoutline  textoutlinecolor  linespacing  textoffy  reservetop  reserveright  reservebottom  reserveleft\n\n";
    cout << "Requires:\n";
    cout << "  ImageMagick  —  brew install imagemagick\n\n";
    cout << "Examples:\n";
    cout << "  textimage \"He is risen.\"\n";
    cout << "  textimage --text=\"He is risen.\" --output=risen.jpg\n";
    cout << "  textimage \"Be still and know that I am God.\" --bg=\"#1a1a2e\" --textcolor=\"#e0e0e0\"\n";
    cout << "  textimage \"Grace upon grace.\" --bgphoto=sky.jpg --dim=60 --textoutline=3\n";
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

#ifdef _WIN32
    const string LP = "(", RP = ")";
#else
    const string LP = "\\(", RP = "\\)";
#endif

    string im = detectIM();

    map<string, string> cfg = loadConfig();

#ifdef __APPLE__
    const string defaultFont = "/System/Library/Fonts/Palatino.ttc";
#elif defined(_WIN32)
    const string defaultFont = "C:/Windows/Fonts/pala.ttf";
#else
    const string defaultFont = "DejaVu-Serif";
#endif

    string inputText;
    string outputFile;
    int imgWidth      = stoi(cfgGet(cfg, "width",     "1920"));
    int imgHeight     = stoi(cfgGet(cfg, "height",    "1080"));
    string font       = cfgGet(cfg, "font",           defaultFont);
    string bgColor    = cfgGet(cfg, "bg",             "black");
    string bgPhoto    = cfgGet(cfg, "bgphoto",        "");
    int    dimPct     = stoi(cfgGet(cfg, "dim",       "50"));
    string textColor  = cfgGet(cfg, "textcolor",      "white");
    int textSizePt       = stoi(cfgGet(cfg, "textsize",      "0"));
    int maxTextSizePt    = stoi(cfgGet(cfg, "maxtextsize",   "0"));
    int textScalePct     = stoi(cfgGet(cfg, "textscale",     "100"));
    int textPanelOpacity = stoi(cfgGet(cfg, "textpanel",     "0"));
    string textPanelColor = cfgGet(cfg, "textpanelcolor",   "black");
    auto parseShadow = [](const string& v) -> int {
        if (v == "yes") return 5;
        if (v == "no" || v.empty()) return 0;
        try { int n = stoi(v); return max(0, min(10, n)); } catch (...) { return 0; }
    };
    int textShadow       = parseShadow(cfgGet(cfg, "textshadow", "no"));
    int shadowMethod     = max(1, min(2, stoi(cfgGet(cfg, "shadowmethod", "1"))));
    int textOutline      = max(0, stoi(cfgGet(cfg, "textoutline",      "0")));
    string textOutlineColor = cfgGet(cfg, "textoutlinecolor", "black");
    bool panelRounded    = cfgGet(cfg, "textpanelrounded",  "no") == "yes";
    int lineSpacing      = stoi(cfgGet(cfg, "linespacing",    "0"));
    int textOffsetY      = stoi(cfgGet(cfg, "textoffy",      "0"));
    int reserveTop    = max(0, min(90, stoi(cfgGet(cfg, "reservetop",    "0"))));
    int reserveRight  = max(0, min(90, stoi(cfgGet(cfg, "reserveright",  "0"))));
    int reserveBottom = max(0, min(90, stoi(cfgGet(cfg, "reservebottom", "0"))));
    int reserveLeft   = max(0, min(90, stoi(cfgGet(cfg, "reserveleft",   "0"))));
    string text2      = cfgGet(cfg, "text2",      "");
    int    text2gap   = stoi(cfgGet(cfg, "text2gap", "40"));
    string text2color = cfgGet(cfg, "text2color", "");   // empty = same as textcolor
    string text2font  = cfgGet(cfg, "text2font",  "");   // empty = same as font
    int    text2Outline      = -1;   // -1 = inherit from textOutline
    string text2OutlineColor = "";   // "" = inherit from textOutlineColor
    int    text2Shadow       = -1;   // -1 = inherit from textShadow
    int    text2ShadowMethod = -1;   // -1 = inherit from shadowMethod

    bool saveConfig  = false;
    bool showConfig  = false;

    // ── Argument parsing ──────────────────────────────────────────────────
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            cout << "textimage v" << TI_VERSION << "\n";
            return 0;
        } else if (arg.find("--text=") == 0) {
            inputText = arg.substr(7);
        } else if (arg.find("--text2=") == 0) {
            text2 = arg.substr(8);
        } else if (arg.find("--text2gap=") == 0) {
            text2gap = max(0, stoi(arg.substr(11)));
        } else if (arg.find("--text2color=") == 0) {
            text2color = arg.substr(13);
        } else if (arg.find("--text2font=") == 0) {
            text2font = arg.substr(12);
        } else if (arg.find("--text2outline=") == 0) {
            text2Outline = max(0, stoi(arg.substr(15)));
        } else if (arg == "--text2outline") {
            text2Outline = 2;
        } else if (arg == "--no-text2outline") {
            text2Outline = 0;
        } else if (arg.find("--text2outlinecolor=") == 0) {
            text2OutlineColor = arg.substr(20);
        } else if (arg.find("--text2shadow=") == 0) {
            text2Shadow = max(0, min(10, stoi(arg.substr(14))));
        } else if (arg == "--text2shadow") {
            text2Shadow = 5;
        } else if (arg == "--no-text2shadow") {
            text2Shadow = 0;
        } else if (arg.find("--text2shadowmethod=") == 0) {
            text2ShadowMethod = max(1, min(2, stoi(arg.substr(20))));
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
        } else if (arg.find("--reserve=") == 0) {
            string val = arg.substr(10);
            size_t comma = val.find(',');
            if (comma != string::npos) {
                string side = val.substr(0, comma);
                int pct = 0;
                try { pct = max(0, min(90, stoi(val.substr(comma + 1)))); } catch (...) {}
                if      (side == "top")    reserveTop    = pct;
                else if (side == "right")  reserveRight  = pct;
                else if (side == "bottom") reserveBottom = pct;
                else if (side == "left")   reserveLeft   = pct;
                else { cerr << "Error: --reserve side must be top, right, bottom, or left.\n"; return 1; }
            } else {
                cerr << "Error: --reserve requires SIDE,PCT format (e.g. --reserve=left,30).\n"; return 1;
            }
        } else if (arg == "--textpanelrounded") {
            panelRounded = true;
        } else if (arg == "--no-textpanelrounded") {
            panelRounded = false;
        } else if (arg == "--saveconfig") {
            saveConfig = true;
        } else if (arg == "--showconfig") {
            showConfig = true;
        } else if (arg.find("-") == 0) {
            cerr << "Error: unknown option '" << arg << "'\n";
            cerr << "Run 'textimage --help' for usage.\n";
            return 1;
        } else if (inputText.empty()) {
            inputText = arg;
        } else {
            cerr << "Error: unexpected argument '" << arg << "'\n";
            cerr << "Run 'textimage --help' for usage.\n";
            return 1;
        }
    }

    if ((textSizePt > 0 || maxTextSizePt > 0) && textScalePct != 100) {
        cerr << "Error: --textsize/--maxtextsize and --textscale cannot be used together.\n";
        return 1;
    }
    // ── --showconfig: print effective settings and exit ───────────────────
    if (showConfig) {
        cout << "Effective settings (config file + command-line):\n";
        cout << "  width            = " << imgWidth  << "\n";
        cout << "  height           = " << imgHeight << "\n";
        cout << "  font             = " << font      << "\n";
        cout << "  bg               = " << bgColor   << "\n";
        cout << "  bgphoto          = " << (bgPhoto.empty() ? "(none)" : bgPhoto) << "\n";
        cout << "  dim              = " << dimPct    << "\n";
        cout << "  textcolor        = " << textColor << "\n";
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
        cout << "  reserve top      = " << (reserveTop    > 0 ? to_string(reserveTop)    : "off") << "\n";
        cout << "  reserve right    = " << (reserveRight  > 0 ? to_string(reserveRight)  : "off") << "\n";
        cout << "  reserve bottom   = " << (reserveBottom > 0 ? to_string(reserveBottom) : "off") << "\n";
        cout << "  reserve left     = " << (reserveLeft   > 0 ? to_string(reserveLeft)   : "off") << "\n";
        cout << "  text2            = " << (text2.empty() ? "(none)" : text2) << "\n";
        cout << "  text2gap         = " << text2gap << "\n";
        cout << "  text2color       = " << (text2color.empty() ? "(same as textcolor)" : text2color) << "\n";
        cout << "  text2font        = " << (text2font.empty()  ? "(same as font)"      : text2font)  << "\n";
        cout << "  text2outline     = " << (text2Outline < 0 ? "(same as textoutline)" : (text2Outline > 0 ? to_string(text2Outline) : "no")) << "\n";
        cout << "  text2outlinecolor= " << (text2OutlineColor.empty() ? "(same as textoutlinecolor)" : text2OutlineColor) << "\n";
        cout << "  text2shadow      = " << (text2Shadow < 0 ? "(same as textshadow)" : (text2Shadow > 0 ? to_string(text2Shadow) : "no")) << "\n";
        cout << "  text2shadowmethod= " << (text2ShadowMethod < 0 ? "(same as shadowmethod)" : to_string(text2ShadowMethod)) << "\n";
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

    // ── --saveconfig: write [textimage] section of .verselumen and exit ──
    if (saveConfig) {
        vector<string> lines = {
            "width            = " + to_string(imgWidth),
            "height           = " + to_string(imgHeight),
            "font             = " + font,
            "bg               = " + bgColor,
            "bgphoto          = " + bgPhoto,
            "dim              = " + to_string(dimPct),
            "textcolor        = " + textColor,
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
            "reservetop       = " + to_string(reserveTop),
            "reserveright     = " + to_string(reserveRight),
            "reservebottom    = " + to_string(reserveBottom),
            "reserveleft      = " + to_string(reserveLeft),
            "text2            = " + text2,
            "text2gap         = " + to_string(text2gap),
            "text2color       = " + text2color,
            "text2font        = " + text2font,
            "text2outline     = " + (text2Outline < 0 ? string("") : (text2Outline > 0 ? to_string(text2Outline) : string("0"))),
            "text2outlinecolor= " + text2OutlineColor,
            "text2shadow      = " + (text2Shadow < 0 ? string("") : (text2Shadow > 0 ? to_string(text2Shadow) : string("no"))),
            "text2shadowmethod= " + (text2ShadowMethod < 0 ? string("") : to_string(text2ShadowMethod))
        };
        if (!writeSection(lines)) { cerr << "Error: could not write '" << CONFIG_FILE << "'.\n"; return 1; }
        cerr << "Saved [" << SECTION << "] to ./" << CONFIG_FILE << "\n";
        return 0;
    }

    if (inputText.empty()) {
        cerr << "Error: no text given.\n";
        cerr << "Usage: textimage \"Text to render\" [OPTIONS]\n";
        cerr << "       textimage --text=\"Text to render\" [OPTIONS]\n";
        cerr << "Example: textimage \"He is risen.\"\n";
        return 1;
    }

    for (size_t pos = 0; (pos = inputText.find("\\n", pos)) != string::npos; )
        inputText.replace(pos, 2, "\n");
    for (size_t pos = 0; (pos = text2.find("\\n", pos)) != string::npos; )
        text2.replace(pos, 2, "\n");

    if (outputFile.empty())
        outputFile = textToFilename(inputText);

    string quotedText = shellQuote(inputText);

    // ── Layout calculations ───────────────────────────────────────────────
    double scale = (double)imgHeight / 1080.0;

    int textW    = (int)(imgWidth  * 0.896 * textScalePct / 100.0);
    int textH    = (int)(imgHeight * 0.741 * textScalePct / 100.0);

    // Reserve: each side independently shrinks the text canvas and shifts the
    // center into the remaining region.  Multiple sides compose correctly.
    // ImageMagick geometry with Center gravity: +X = right, +Y = down.
    double fracTop    = reserveTop    / 100.0;
    double fracRight  = reserveRight  / 100.0;
    double fracBottom = reserveBottom / 100.0;
    double fracLeft   = reserveLeft   / 100.0;
    int reserveShiftX = (int)(imgWidth  * (fracLeft - fracRight) / 2.0);
    int reserveShiftY = (int)(imgHeight * (fracTop  - fracBottom) / 2.0);
    textW = max(1, (int)(textW * (1.0 - fracLeft - fracRight)));
    textH = max(1, (int)(textH * (1.0 - fracTop  - fracBottom)));
    // Unified geometry: combines --textoffy and reserve shift.
    // In ImageMagick geometry with Center gravity: +X = right, +Y = down.
    int geomX = reserveShiftX;
    int geomY = textOffsetY + reserveShiftY;  // textOffsetY: positive = down
    string textGeom = (geomX >= 0 ? "+" : "-") + to_string(abs(geomX))
                    + (geomY >= 0 ? "+" : "-") + to_string(abs(geomY));

    int borderH = max(10, (int)(20 * scale));
    int borderW = max(20, (int)(40 * scale));

    // When stacking two texts, give each block half the available height minus the gap.
    int textH_each = text2.empty() ? textH : max(1, (textH - max(0, text2gap)) / 2);

    if (im.empty()) {
        cerr << "Error: ImageMagick not found. Install it with:\n";
        cerr << "  macOS:   brew install imagemagick\n";
        cerr << "  Linux:   apt install imagemagick\n";
        cerr << "  Windows: choco install imagemagick\n";
        return 1;
    }

    string layerBg = (bgPhoto.empty() && textShadow == 0 && textOutline == 0) ? bgColor : "none";

    // Determine pointsize (fixed, capped, or auto-fit).
    bool applyPointsize  = false;
    int  applyPointsizeN = 0;
    if (maxTextSizePt > 0) {
        ostringstream qcmd;
        qcmd << im
             << " -background \"" << layerBg << "\""
             << " -fill \""       << textColor << "\""
             << " -font \""       << font << "\""
             << " -gravity Center"
             << (lineSpacing != 0 ? " -interline-spacing " + to_string(lineSpacing) : "")
             << " -size "         << textW << "x" << textH_each
             << " caption:"       << quotedText
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
        applyPointsize  = true;
        applyPointsizeN = textSizePt;
    }

    // Temp files for intermediate layers.
    string tmpLayer    = outputFile + ".tmp_layer.png";
    string tmpOutline  = outputFile + ".tmp_outline.png";
    string tmpShadow   = outputFile + ".tmp_shadow.png";
    string tmpOutline2 = outputFile + ".tmp_outline2.png";
    string tmpShadow2  = outputFile + ".tmp_shadow2.png";

    // Effective text2 effect settings (inherit from text1 when not overridden)
    int    eff2Outline      = (text2Outline >= 0)      ? text2Outline      : textOutline;
    string eff2OutlineColor = text2OutlineColor.empty() ? textOutlineColor  : text2OutlineColor;
    int    eff2Shadow       = (text2Shadow >= 0)        ? text2Shadow       : textShadow;
    int    eff2ShadowMethod = (text2ShadowMethod >= 0)  ? text2ShadowMethod : shadowMethod;

    // Apply outline to a layer file, replacing it on success (old file is deleted)
    auto applyOutlineToLayer = [&](string& layer, int outline, const string& outColor,
                                   const string& tmpOut) {
        if (outline <= 0) return;
        ostringstream cmd;
        cmd << im << " \"" << layer << "\""
            << " " << LP << " +clone -channel alpha"
            << " -morphology Dilate disk:" << outline
            << " +channel -fill \"" << outColor << "\" -colorize 100 " << RP
            << " +swap -composite"
            << " \"" << tmpOut << "\"";
        if (runSystem(cmd.str()) == 0) { remove(layer.c_str()); layer = tmpOut; }
    };

    // Apply shadow to a layer file, replacing it on success (old file is deleted)
    auto applyShadowToLayer = [&](string& layer, int shadow, int method,
                                  const string& tmpOut) {
        if (shadow <= 0) return;
        double sigma   = (method == 1) ? shadow * 0.8 : 0.0;
        int    offset  = max(1, (int)round(shadow * 0.6));
        int    opacity = (method == 1) ? 80 : 100;
        ostringstream cmd;
        cmd << im << " \"" << layer << "\""
            << " " << LP << " +clone -background black -shadow " << opacity << "x" << sigma
            << "+" << offset << "+" << offset << " " << RP
            << " +swap -background none -flatten"
            << " \"" << tmpOut << "\"";
        if (runSystem(cmd.str()) == 0) { remove(layer.c_str()); layer = tmpOut; }
    };

    // ── Step 1: Render text1 to a trimmed PNG layer ────────────────────────
    ostringstream cmd1;
    cmd1 << im
         << " -background \"" << layerBg << "\""
         << " -fill \""       << textColor << "\""
         << " -font \""       << font << "\""
         << " -gravity Center"
         << (lineSpacing != 0 ? " -interline-spacing " + to_string(lineSpacing) : "")
         << " -size "         << textW << "x" << textH_each
         << (applyPointsize ? " -pointsize " + to_string(applyPointsizeN) : "")
         << " caption:"       << quotedText
         << " -trim"
         << " -bordercolor \"" << layerBg << "\""
         << " -border " << borderW << "x" << borderH
         << " \"" << tmpLayer << "\"";

    int ret1 = runSystem(cmd1.str());
    if (ret1 != 0) {
        cerr << "Error: text layer generation failed.\n\n";
        cerr << "To list fonts or use a font by path:\n";
        cerr << "  magick -list font\n";
        cerr << "  textimage \"...\" --font=\"/path/to/font.ttf\"\n";
        return 1;
    }

    // Measure pre-effect height of text1 (shadow expands the canvas, so capture now)
    int baseH1 = 0;
    if (!text2.empty()) {
        int _bw = 0;
        string _bc = im + " identify -format \"%wx%h\" \"" + tmpLayer + "\"";
        FILE* _bp = runPopen(_bc, "r");
        if (_bp) { char buf[64] = {}; fgets(buf, sizeof(buf), _bp); pclose(_bp);
                   sscanf(buf, "%dx%d", &_bw, &baseH1); }
    }

    // Apply text1 outline then shadow
    applyOutlineToLayer(tmpLayer, textOutline, textOutlineColor, tmpOutline);
    applyShadowToLayer(tmpLayer, textShadow, shadowMethod, tmpShadow);

    // ── Step 1b: Render text2, apply its effects, combine ─────────────────
    if (!text2.empty()) {
        string tmpL2 = outputFile + ".tmp_layer2.png";
        string tmpLC = outputFile + ".tmp_combined.png";
        string quotedText2 = shellQuote(text2);

        const string& t2color = text2color.empty() ? textColor : text2color;
        const string& t2font  = text2font.empty()  ? font      : text2font;
        ostringstream cmd1b;
        cmd1b << im
              << " -background \"" << layerBg << "\""
              << " -fill \""       << t2color << "\""
              << " -font \""       << t2font  << "\""
              << " -gravity Center"
              << (lineSpacing != 0 ? " -interline-spacing " + to_string(lineSpacing) : "")
              << " -size "         << textW << "x" << textH_each
              << (applyPointsize ? " -pointsize " + to_string(applyPointsizeN) : "")
              << " caption:"       << quotedText2
              << " -trim"
              << " -bordercolor \"" << layerBg << "\""
              << " -border " << borderW << "x" << borderH
              << " \"" << tmpL2 << "\"";

        if (runSystem(cmd1b.str()) == 0) {
            // Apply text2 outline then shadow
            applyOutlineToLayer(tmpL2, eff2Outline, eff2OutlineColor, tmpOutline2);
            applyShadowToLayer(tmpL2, eff2Shadow, eff2ShadowMethod, tmpShadow2);

            // Query pixel dimensions of both processed layers
            auto identDim = [&](const string& path, int& w, int& h) {
                string cmd = im + " identify -format \"%wx%h\" \"" + path + "\"";
                FILE* p = runPopen(cmd, "r");
                if (p) { char buf[64] = {}; fgets(buf, sizeof(buf), p); pclose(p);
                         sscanf(buf, "%dx%d", &w, &h); }
            };
            int W1 = 0, H1 = 0, W2 = 0, H2 = 0;
            identDim(tmpLayer, W1, H1);
            identDim(tmpL2,    W2, H2);

            int combinedW = max(W1, W2);
            // Use pre-effect height for gap so shadow expansion doesn't widen it
            int y2        = max(0, baseH1 - 2 * borderH + text2gap);
            int combinedH = max(H1, y2 + H2);  // H1 may be larger due to shadow
            int x1 = (combinedW - W1) / 2;
            int x2 = (combinedW - W2) / 2;

            ostringstream cmd1c;
            cmd1c << im
                  << " -size " << combinedW << "x" << combinedH
                  << " xc:\"" << layerBg << "\""
                  << " \"" << tmpLayer << "\" -gravity NorthWest"
                  << " -geometry +" << x1 << "+0 -composite"
                  << " \"" << tmpL2 << "\" -gravity NorthWest"
                  << " -geometry +" << x2 << "+" << y2 << " -composite"
                  << " \"" << tmpLC << "\"";

            if (runSystem(cmd1c.str()) == 0) {
                remove(tmpLayer.c_str());
                remove(tmpL2.c_str());
                tmpLayer = tmpLC;
            } else {
                remove(tmpL2.c_str());
            }
        }
    }

    string activeLayer = tmpLayer;

    // ── Query layer dimensions (for panel) ────────────────────────────────
    int layerW = 0, layerH = 0;
    if (textPanelOpacity > 0) {
        string identCmd = im + " identify -format \"%wx%h\" \"" + tmpLayer + "\"";
        FILE* pipe = runPopen(identCmd, "r");
        if (pipe) {
            char buf[64] = {};
            fgets(buf, sizeof(buf), pipe);
            pclose(pipe);
            sscanf(buf, "%dx%d", &layerW, &layerH);
        }
    }

    // ── Panel fragment ────────────────────────────────────────────────────
    ostringstream panelDraw;
    if (textPanelOpacity > 0 && layerW > 0 && layerH > 0) {
        int panelH = layerH;
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
                  << " -geometry " << textGeom
                  << " -composite";
    }

    // ── Step 2: Composite text layer onto background ───────────────────────
    ostringstream cmd2;
    if (bgPhoto.empty()) {
        cmd2 << im
             << " -size " << imgWidth << "x" << imgHeight
             << " xc:\"" << bgColor << "\""
             << panelDraw.str()
             << " \"" << activeLayer << "\""
             << " -gravity Center"
             << " -geometry " << textGeom
             << " -composite"
             << " \"" << outputFile << "\"";
    } else {
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
             << " -geometry " << textGeom
             << " -composite"
             << " \"" << outputFile << "\"";
    }

    int ret2 = runSystem(cmd2.str());

    remove(tmpLayer.c_str());

    if (ret2 != 0) {
        cerr << "Error: image generation failed.\n\n";
        cerr << "To list fonts or use a font by path:\n";
        cerr << "  magick -list font\n";
        cerr << "  textimage \"...\" --font=\"/path/to/font.ttf\"\n";
        return 1;
    }

    cerr << "Saved to " << outputFile << "\n";
    return 0;
}
