// biblereader.cpp — Bible reader in the browser
// Build: g++ -std=c++11 -o biblereader biblereader.cpp
// Usage: ./biblereader [-bv=KJV|BSB|WEB] [--port=7778]
//
// Opens a local browser Bible reader. Click a verse to copy its reference
// to the clipboard. The last selected reference is printed to stdout on exit.

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <csignal>
#ifdef _WIN32
# include <windows.h>
# include <direct.h>   // _getcwd, _mkdir
# include <process.h>  // _spawnlp, _P_NOWAIT
typedef intptr_t pid_t;
#else
# include <unistd.h>
# include <sys/wait.h>
#endif

#ifdef _WIN32
# define HOME_ENV "USERPROFILE"
#else
# define HOME_ENV "HOME"
#endif

using namespace std;

// ── Bible data ────────────────────────────────────────────────────────────────

static vector<string>           gBooks;
static map<string, vector<int>> gChapters;   // book     → sorted chapter list
static map<string, vector<int>> gVerses;     // "Book N" → sorted verse list
static map<string, string>      gText;       // "Book N:V" → verse text

static void loadBible(const string& path) {
    ifstream f(path);
    string line;
    set<string> seen;
    map<string, set<int>> chapSet, verseSet;

    while (getline(f, line)) {
        if (line.size() >= 3 &&
            (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB &&
            (unsigned char)line[2] == 0xBF)
            line = line.substr(3);
        if (!line.empty() && line.back() == '\r') line.pop_back();

        size_t tab = line.find('\t');
        if (tab == string::npos) continue;
        string ref  = line.substr(0, tab);
        string text = line.substr(tab + 1);
        size_t col  = ref.rfind(':');
        if (col == string::npos) continue;
        string bc   = ref.substr(0, col);
        int    v    = stoi(ref.substr(col + 1));
        size_t sp   = bc.rfind(' ');
        if (sp == string::npos) continue;
        string book = bc.substr(0, sp);
        int    ch   = stoi(bc.substr(sp + 1));

        if (!seen.count(book)) { seen.insert(book); gBooks.push_back(book); }
        chapSet[book].insert(ch);
        verseSet[bc].insert(v);
        gText[ref] = text;
    }

    for (auto it = chapSet.begin();  it != chapSet.end();  ++it)
        gChapters[it->first] = vector<int>(it->second.begin(), it->second.end());
    for (auto it = verseSet.begin(); it != verseSet.end(); ++it)
        gVerses[it->first]   = vector<int>(it->second.begin(), it->second.end());
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static string base64Encode(const string& src) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string r;
    r.reserve((src.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 3 <= src.size(); i += 3) {
        unsigned char a = src[i], b = src[i+1], c = src[i+2];
        r += T[a>>2]; r += T[((a&3)<<4)|(b>>4)];
        r += T[((b&15)<<2)|(c>>6)]; r += T[c&63];
    }
    if (i + 1 == src.size()) {
        unsigned char a = src[i];
        r += T[a>>2]; r += T[(a&3)<<4]; r += "==";
    } else if (i + 2 == src.size()) {
        unsigned char a = src[i], b = src[i+1];
        r += T[a>>2]; r += T[((a&3)<<4)|(b>>4)]; r += T[(b&15)<<2]; r += '=';
    }
    return r;
}

static string htmlEsc(const string& s) {
    string r;
    for (char c : s) {
        if      (c == '&') r += "&amp;";
        else if (c == '<') r += "&lt;";
        else if (c == '>') r += "&gt;";
        else if (c == '"') r += "&quot;";
        else               r += c;
    }
    return r;
}

static string jsEsc(const string& s) {
    string r;
    for (char c : s) {
        if      (c == '"')  r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "";
        else                r += c;
    }
    return r;
}

// ── Process management ────────────────────────────────────────────────────────

static string g_tempDir;
static bool   g_cleaned = false;
#ifdef _WIN32
static HANDLE g_serverHandle = NULL;   // _spawnlp _P_NOWAIT returns a HANDLE, not a PID
#else
static pid_t  g_serverPid = -1;
#endif

static void doCleanup() {
    if (g_cleaned) return;
    g_cleaned = true;
#ifdef _WIN32
    if (g_serverHandle) {
        TerminateProcess(g_serverHandle, 1);
        CloseHandle(g_serverHandle);
        g_serverHandle = NULL;
    }
    if (!g_tempDir.empty()) {
        system(("rd /s /q \"" + g_tempDir + "\"").c_str());
        g_tempDir.clear();
    }
#else
    if (g_serverPid > 0) {
        kill(g_serverPid, SIGTERM);
        waitpid(g_serverPid, nullptr, WNOHANG);
        g_serverPid = -1;
    }
    if (!g_tempDir.empty()) {
        system(("rm -rf \"" + g_tempDir + "\"").c_str());
        g_tempDir.clear();
    }
#endif
}

static void sigHandler(int) { doCleanup(); exit(0); }

// ── Python server ─────────────────────────────────────────────────────────────

// HTML is base64-embedded in server.py to avoid AV quarantine of .html files in %TEMP%
static string makePythonServer(const string& html) {
    ostringstream py;
    py << "#!/usr/bin/env python3\n"
          "import http.server, urllib.parse, os, sys, base64\n"
          "\n"
          "PORT    = int(sys.argv[1]) if len(sys.argv) > 1 else 7778\n"
          "BASE    = os.path.dirname(os.path.abspath(__file__))\n"
          "SELFILE = os.path.join(BASE, 'selected.txt')\n"
          "_HTML   = base64.b64decode(b\""
       << base64Encode(html)
       << "\")\n"
          "\n"
          "def serve_file(handler, rel_path, content_type):\n"
          "    path = os.path.join(BASE, rel_path)\n"
          "    try:\n"
          "        with open(path, 'rb') as f:\n"
          "            data = f.read()\n"
          "        handler.send_response(200)\n"
          "        handler.send_header('Content-Type', content_type)\n"
          "        handler.send_header('Content-Length', str(len(data)))\n"
          "        handler.end_headers()\n"
          "        handler.wfile.write(data)\n"
          "    except Exception as e:\n"
          "        handler.send_error(500, str(e))\n"
          "\n"
          "class Handler(http.server.BaseHTTPRequestHandler):\n"
          "    def do_GET(self):\n"
          "        p = urllib.parse.urlparse(self.path).path\n"
          "        if p.startswith('/pick'):\n"
          "            q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)\n"
          "            if 'ref' in q:\n"
          "                with open(SELFILE, 'w') as f:\n"
          "                    f.write(q['ref'][0])\n"
          "            self.send_response(200)\n"
          "            self.end_headers()\n"
          "            self.wfile.write(b'ok')\n"
          "        elif p in ('/', '/index.html'):\n"
          "            self.send_response(200)\n"
          "            self.send_header('Content-Type', 'text/html; charset=utf-8')\n"
          "            self.send_header('Content-Length', str(len(_HTML)))\n"
          "            self.end_headers()\n"
          "            self.wfile.write(_HTML)\n"
          "        elif p.startswith('/Bible') and p.endswith('.txt'):\n"
          "            serve_file(self, os.path.basename(p), 'text/plain; charset=utf-8')\n"
          "        else:\n"
          "            self.send_error(404)\n"
          "    def log_message(self, *a):\n"
          "        pass\n"
          "\n"
          "with http.server.HTTPServer(('127.0.0.1', PORT), Handler) as httpd:\n"
          "    httpd.serve_forever()\n";
    return py.str();
}

// ── HTML / JS generation ──────────────────────────────────────────────────────

static string makeBibleJS() {
    ostringstream js;

    js << "const _INIT_BOOKS=[";
    for (size_t i = 0; i < gBooks.size(); ++i) {
        if (i) js << ",";
        js << "\"" << jsEsc(gBooks[i]) << "\"";
    }
    js << "];\n";

    js << "const _INIT_BIBLE={";
    for (size_t bi = 0; bi < gBooks.size(); ++bi) {
        const string& book = gBooks[bi];
        if (bi) js << ",";
        js << "\"" << jsEsc(book) << "\":{";
        const vector<int>& chs = gChapters[book];
        for (size_t ci = 0; ci < chs.size(); ++ci) {
            int ch = chs[ci];
            if (ci) js << ",";
            js << ch << ":{";
            string bc = book + " " + to_string(ch);
            const vector<int>& vs = gVerses[bc];
            for (size_t vi = 0; vi < vs.size(); ++vi) {
                int v = vs[vi];
                if (vi) js << ",";
                string ref = bc + ":" + to_string(v);
                js << v << ":\"" << jsEsc(gText[ref]) << "\"";
            }
            js << "}";
        }
        js << "}";
    }
    js << "};\n";

    return js.str();
}

static string makeHtml(const string& version, bool copyVerse, const vector<string>& available) {
    ostringstream html;

    html << R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Bible Reader</title>
<style>
:root{--bg:#0d1117;--sb:#161b22;--text:#e6edf3;--dim:#8b949e;--accent:#58a6ff;
  --border:#30363d;--hover:#1c2d3f;--sel:#1a3a6b;--vn:#8b949e}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:system-ui,sans-serif;
  display:flex;flex-direction:column;height:100vh;overflow:hidden}

/* ── header ── */
#hdr{display:flex;align-items:center;gap:12px;padding:10px 16px;
  border-bottom:1px solid var(--border);flex-shrink:0;background:var(--sb)}
#hdr-title{font-weight:600;font-size:15px;white-space:nowrap}
#hdr-ver{font-size:12px;color:var(--dim);white-space:nowrap}
#search{flex:1;background:var(--bg);color:var(--text);border:1px solid var(--border);
  border-radius:6px;padding:6px 10px;font-size:13px;outline:none}
#search:focus{border-color:var(--accent)}
#search-clear{background:none;border:none;color:var(--dim);cursor:pointer;
  font-size:16px;padding:0 4px;display:none}
#search-clear:hover{color:var(--text)}

/* ── layout ── */
#body{display:flex;flex:1;overflow:hidden}

/* ── sidebar ── */
#sidebar{width:190px;flex-shrink:0;overflow-y:auto;border-right:1px solid var(--border);
  background:var(--sb);padding:6px 0}
#sidebar::-webkit-scrollbar{width:6px}
#sidebar::-webkit-scrollbar-thumb{background:var(--border);border-radius:3px}
.book{border-bottom:1px solid #1c2128}
.bname{padding:5px 12px;font-size:13px;cursor:pointer;user-select:none;
  white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.bname:hover{background:var(--hover);color:var(--accent)}
.bname.open{color:var(--accent)}
.bchaps{display:none;padding:4px 8px 6px 12px;flex-wrap:wrap;gap:3px}
.bchaps.open{display:flex}
.chn{min-width:24px;padding:2px 4px;font-size:11px;text-align:center;cursor:pointer;
  border-radius:4px;color:var(--dim);user-select:none}
.chn:hover{background:var(--hover);color:var(--text)}
.chn.active{background:var(--accent);color:#0d1117;font-weight:600}

/* ── main ── */
#main{flex:1;overflow-y:auto;padding:0}
#main::-webkit-scrollbar{width:8px}
#main::-webkit-scrollbar-thumb{background:var(--border);border-radius:4px}
#ch-hdr{display:flex;align-items:center;gap:8px;padding:14px 28px 10px;
  border-bottom:1px solid var(--border);position:sticky;top:0;
  background:var(--bg);z-index:5}
#ch-title{font-size:18px;font-weight:600;flex:1}
.nav-btn{background:none;border:1px solid var(--border);color:var(--dim);
  border-radius:6px;padding:4px 10px;font-size:13px;cursor:pointer}
.nav-btn:hover{border-color:var(--accent);color:var(--accent)}
.nav-btn:disabled{opacity:0.25;cursor:default}
#text-block{padding:20px 28px 40px;line-height:1.85;font-size:16px;
  font-family:Georgia,'Times New Roman',serif}
.verse{cursor:pointer;border-radius:3px;padding:1px 2px;transition:background .1s}
.verse:hover{background:var(--hover)}
.verse.sel{background:var(--sel)}
sup.vn{font-size:10px;color:var(--vn);margin-right:2px;margin-left:3px;
  font-family:system-ui,sans-serif;user-select:none;vertical-align:super}

/* ── search results ── */
#results{padding:16px 28px;display:none}
.r-count{font-size:12px;color:var(--dim);margin-bottom:12px}
.r-item{padding:8px 0;border-bottom:1px solid var(--border);cursor:pointer}
.r-item:hover .r-ref{color:var(--accent)}
.r-ref{font-size:12px;color:var(--dim);font-family:monospace;margin-bottom:3px}
.r-text{font-size:14px;font-family:Georgia,serif;line-height:1.5}
.r-text em{background:#2d3e1f;color:#7ee787;font-style:normal;border-radius:2px;padding:0 2px}

/* ── status bar ── */
#bar{flex-shrink:0;height:38px;background:#010409;border-top:1px solid var(--border);
  display:flex;align-items:center;gap:10px;padding:0 16px;overflow:hidden}
#bar-ref{font-family:monospace;font-size:13px;color:var(--accent);
  overflow:hidden;text-overflow:ellipsis;white-space:nowrap;flex:1}
#bar-hint{font-size:11px;color:var(--dim);white-space:nowrap}
#bar-ok{color:#3fb950;font-size:12px;opacity:0;transition:opacity .3s;white-space:nowrap}
#ver-sel{display:flex;gap:4px;flex-shrink:0}
.ver-btn{background:none;border:1px solid var(--border);color:var(--dim);border-radius:5px;
  padding:4px 9px;font-size:12px;cursor:pointer;font-weight:500}
.ver-btn:hover{border-color:var(--accent);color:var(--text)}
.ver-btn.active{background:var(--accent);border-color:var(--accent);color:#0d1117}
#loading{display:none;position:fixed;inset:0;background:rgba(13,17,23,.75);
  z-index:100;align-items:center;justify-content:center;font-size:14px;color:var(--dim)}
</style>
</head>
<body>

<div id="hdr">
  <div id="hdr-title">Bible Reader</div>
  <div id="hdr-ver">)html" << htmlEsc(version) << R"html(</div>
  <input id="search" type="text" placeholder="Search verses&#x2026;" oninput="onSearch()" spellcheck="false">
  <button id="search-clear" onclick="clearSearch()">&#x2715;</button>
  <div id="ver-sel"></div>
</div>
<div id="loading">Loading&#x2026;</div>

<div id="body">
  <div id="sidebar"></div>
  <div id="main">
    <div id="ch-hdr" style="display:none">
      <button class="nav-btn" id="btn-prev" onclick="navChapter(-1)">&#x2190; Prev</button>
      <div id="ch-title"></div>
      <button class="nav-btn" id="btn-next" onclick="navChapter(1)">Next &#x2192;</button>
    </div>
    <div id="text-block"></div>
    <div id="results"></div>
  </div>
</div>

<div id="bar">
  <div id="bar-ref">Click a verse to select it</div>
  <div id="bar-hint"></div>
  <div id="bar-ok">&#x2713; copied</div>
</div>

<script>
)html" << makeBibleJS()
      << "const INIT_VER=\"" << jsEsc(version) << "\";\n"
      << [&]() {
             string s = "const AVAIL_VERS=[";
             for (size_t i = 0; i < available.size(); ++i) {
                 if (i) s += ",";
                 s += "\"" + available[i] + "\"";
             }
             return s + "];\n";
         }()
      << "const COPY_VERSE=" << (copyVerse ? "true" : "false") << ";\n"
      << "let BOOKS=_INIT_BOOKS,BIBLE=_INIT_BIBLE,currentVer=INIT_VER;\n"
         "const _verCache={[INIT_VER]:{books:_INIT_BOOKS,bible:_INIT_BIBLE}};\n"
      << R"html(

// ── state ──────────────────────────────────────────────────────────────────
let curBook = null, curCh = null;
let searchTimer = null;

function sanitizeId(s) { return s.replace(/[^a-zA-Z0-9]/g, '_'); }

function esc(s) {
    return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

// ── sidebar ────────────────────────────────────────────────────────────────
function buildSidebar() {
    const sb = document.getElementById('sidebar');
    BOOKS.forEach(book => {
        const chs = Object.keys(BIBLE[book]).map(Number).sort((a,b)=>a-b);

        const bookDiv  = document.createElement('div');
        bookDiv.className = 'book';

        const nameDiv  = document.createElement('div');
        nameDiv.className = 'bname';
        nameDiv.textContent = book;
        nameDiv.addEventListener('click', () => toggleBook(book));

        const chapsDiv = document.createElement('div');
        chapsDiv.className = 'bchaps';
        chapsDiv.id = 'bc-' + sanitizeId(book);

        chs.forEach(ch => {
            const span = document.createElement('span');
            span.className = 'chn';
            span.textContent = ch;
            span.id = 'chn-' + sanitizeId(book) + '-' + ch;
            span.addEventListener('click', e => { e.stopPropagation(); showChapter(book, ch); });
            chapsDiv.appendChild(span);
        });

        bookDiv.appendChild(nameDiv);
        bookDiv.appendChild(chapsDiv);
        sb.appendChild(bookDiv);
    });
}

function toggleBook(book) {
    const chapsDiv = document.getElementById('bc-' + sanitizeId(book));
    const nameDiv  = chapsDiv.previousElementSibling;
    const open = chapsDiv.classList.toggle('open');
    nameDiv.classList.toggle('open', open);
}

function ensureBookOpen(book) {
    const chapsDiv = document.getElementById('bc-' + sanitizeId(book));
    if (!chapsDiv.classList.contains('open')) toggleBook(book);
    // Scroll sidebar to book
    chapsDiv.parentElement.scrollIntoView({block:'nearest'});
}

// ── chapter view ───────────────────────────────────────────────────────────
function showChapter(book, ch, targetVerse) {
    clearSearch();
    curBook = book; curCh = ch;

    // Sidebar highlight
    document.querySelectorAll('.chn.active').forEach(e => e.classList.remove('active'));
    const chnEl = document.getElementById('chn-' + sanitizeId(book) + '-' + ch);
    if (chnEl) { chnEl.classList.add('active'); ensureBookOpen(book); }

    // Header
    document.getElementById('ch-hdr').style.display = 'flex';
    document.getElementById('ch-title').textContent  = book + ' ' + ch;
    updateNavButtons();

    // Verse text
    const chData = BIBLE[book][ch];
    const nums   = Object.keys(chData).map(Number).sort((a,b)=>a-b);
    const tb     = document.getElementById('text-block');
    tb.innerHTML = '';
    nums.forEach(v => {
        const ref  = book + ' ' + ch + ':' + v;
        const span = document.createElement('span');
        span.className = 'verse';
        span.dataset.ref = ref;
        span.innerHTML = '<sup class="vn">' + v + '</sup>' + esc(chData[v]) + ' ';
        span.addEventListener('click', () => pickVerse(span));
        tb.appendChild(span);
    });

    document.getElementById('results').style.display = 'none';
    tb.style.display = 'block';

    if (targetVerse) {
        const el = tb.querySelector('[data-ref="' + targetVerse + '"]');
        if (el) { el.classList.add('sel'); el.scrollIntoView({block:'center'}); }
        else     { tb.parentElement.scrollTop = 0; }
    } else {
        tb.parentElement.scrollTop = 0;
    }
}

function updateNavButtons() {
    const bi  = BOOKS.indexOf(curBook);
    const chs = Object.keys(BIBLE[curBook]).map(Number).sort((a,b)=>a-b);
    const ci  = chs.indexOf(curCh);
    document.getElementById('btn-prev').disabled = (bi === 0 && ci === 0);
    document.getElementById('btn-next').disabled = (bi === BOOKS.length-1 && ci === chs.length-1);
}

function navChapter(dir) {
    const chs = Object.keys(BIBLE[curBook]).map(Number).sort((a,b)=>a-b);
    let ci = chs.indexOf(curCh) + dir;
    let bi = BOOKS.indexOf(curBook);
    if (ci < 0) {
        bi--;
        if (bi < 0) return;
        const pchs = Object.keys(BIBLE[BOOKS[bi]]).map(Number).sort((a,b)=>a-b);
        showChapter(BOOKS[bi], pchs[pchs.length-1]);
    } else if (ci >= chs.length) {
        bi++;
        if (bi >= BOOKS.length) return;
        const nchs = Object.keys(BIBLE[BOOKS[bi]]).map(Number).sort((a,b)=>a-b);
        showChapter(BOOKS[bi], nchs[0]);
    } else {
        showChapter(curBook, chs[ci]);
    }
}

// ── verse pick ─────────────────────────────────────────────────────────────
function pickVerse(el) {
    document.querySelectorAll('.verse.sel').forEach(e => e.classList.remove('sel'));
    el.classList.add('sel');
    const ref     = el.dataset.ref;
    const verseText = el.innerText.replace(/^\d+\s*/, '').trim();
    const payload = COPY_VERSE ? verseText : ref;
    document.getElementById('bar-ref').textContent  = ref;
    document.getElementById('bar-hint').textContent = COPY_VERSE ? 'verse text copied' : 'reference copied to clipboard';
    navigator.clipboard.writeText(payload).then(() => {
        const ok = document.getElementById('bar-ok');
        ok.style.opacity = 1;
        setTimeout(() => ok.style.opacity = 0, 1500);
    }).catch(()=>{});
    fetch('/pick?ref=' + encodeURIComponent(payload)).catch(()=>{});
}

// ── search ─────────────────────────────────────────────────────────────────
function onSearch() {
    const q = document.getElementById('search').value.trim();
    document.getElementById('search-clear').style.display = q ? 'inline' : 'none';
    clearTimeout(searchTimer);
    if (!q) { restoreChapter(); return; }
    searchTimer = setTimeout(() => doSearch(q), 250);
}

function clearSearch() {
    document.getElementById('search').value = '';
    document.getElementById('search-clear').style.display = 'none';
    restoreChapter();
}

function restoreChapter() {
    document.getElementById('results').style.display = 'none';
    document.getElementById('text-block').style.display = curBook ? 'block' : 'none';
    document.getElementById('ch-hdr').style.display = curBook ? 'flex' : 'none';
}

function doSearch(q) {
    const ql = q.toLowerCase();
    const items = [];
    const MAX = 300;
    outer:
    for (const book of BOOKS) {
        const bdata = BIBLE[book];
        for (const ch in bdata) {
            const cdata = bdata[ch];
            for (const v in cdata) {
                const text = cdata[v];
                if (text.toLowerCase().includes(ql)) {
                    items.push({book, ch:parseInt(ch), v:parseInt(v), text,
                                ref: book+' '+ch+':'+v});
                    if (items.length >= MAX) break outer;
                }
            }
        }
    }

    const rd = document.getElementById('results');
    rd.innerHTML = '';
    rd.style.display = 'block';
    document.getElementById('text-block').style.display = 'none';
    document.getElementById('ch-hdr').style.display = 'none';

    const count = document.createElement('div');
    count.className = 'r-count';
    count.textContent = items.length === MAX
        ? 'Showing first ' + MAX + ' results for “' + q + '”'
        : items.length + ' result' + (items.length!==1?'s':'') + ' for “' + q + '”';
    rd.appendChild(count);

    if (!items.length) return;

    // Highlight matches in text
    const re = new RegExp('(' + q.replace(/[.*+?^${}()|[\]\\]/g,'\\$&') + ')', 'gi');
    items.forEach(({book, ch, v, text, ref}) => {
        const item = document.createElement('div');
        item.className = 'r-item';
        const refDiv  = document.createElement('div');
        refDiv.className = 'r-ref';
        refDiv.textContent = ref;
        const textDiv = document.createElement('div');
        textDiv.className = 'r-text';
        textDiv.innerHTML = esc(text).replace(re, '<em>$1</em>');
        item.appendChild(refDiv);
        item.appendChild(textDiv);
        item.addEventListener('click', () => showChapter(book, ch, ref));
        rd.appendChild(item);
    });
}

// ── keyboard navigation ────────────────────────────────────────────────────
document.addEventListener('keydown', e => {
    if (document.getElementById('search') === document.activeElement) return;
    if (e.key === 'ArrowLeft'  || (e.key === 'ArrowUp'   && e.altKey)) navChapter(-1);
    if (e.key === 'ArrowRight' || (e.key === 'ArrowDown'  && e.altKey)) navChapter(1);
});

// ── version switching ──────────────────────────────────────────────────────
function buildVerSelector() {
    if (AVAIL_VERS.length < 2) return;
    const div = document.getElementById('ver-sel');
    AVAIL_VERS.forEach(ver => {
        const btn = document.createElement('button');
        btn.className = 'ver-btn' + (ver === currentVer ? ' active' : '');
        btn.textContent = ver;
        btn.addEventListener('click', () => switchVersion(ver));
        div.appendChild(btn);
    });
}

function parseBible(text) {
    const books = [], bible = {};
    const seen = new Set();
    for (const line of text.split('\n')) {
        const tab = line.indexOf('\t');
        if (tab < 0) continue;
        const ref   = line.substring(0, tab);
        const vtext = line.substring(tab + 1).replace(/\r$/, '');
        const col = ref.lastIndexOf(':');
        if (col < 0) continue;
        const bc = ref.substring(0, col);
        const v  = parseInt(ref.substring(col + 1));
        if (isNaN(v)) continue;
        const sp = bc.lastIndexOf(' ');
        if (sp < 0) continue;
        const book = bc.substring(0, sp);
        const ch   = parseInt(bc.substring(sp + 1));
        if (isNaN(ch)) continue;
        if (!seen.has(book)) { seen.add(book); books.push(book); bible[book] = {}; }
        if (!bible[book][ch]) bible[book][ch] = {};
        bible[book][ch][v] = vtext;
    }
    return { books, bible };
}

async function switchVersion(ver) {
    if (ver === currentVer) return;
    if (_verCache[ver]) { applyVersion(ver, _verCache[ver].books, _verCache[ver].bible); return; }
    const ld = document.getElementById('loading');
    ld.style.display = 'flex';
    try {
        const r = await fetch('/Bible' + ver + '.txt');
        if (!r.ok) throw new Error();
        const { books, bible } = parseBible(await r.text());
        _verCache[ver] = { books, bible };
        applyVersion(ver, books, bible);
    } catch(e) { alert('Could not load ' + ver + ' Bible.'); }
    finally    { ld.style.display = 'none'; }
}

function applyVersion(ver, books, bible) {
    BOOKS = books; BIBLE = bible; currentVer = ver;
    document.getElementById('hdr-ver').textContent = ver;
    document.getElementById('sidebar').innerHTML = '';
    buildSidebar();
    document.querySelectorAll('.ver-btn').forEach(b => b.classList.toggle('active', b.textContent === ver));
    if (curBook && BIBLE[curBook] && BIBLE[curBook][curCh]) showChapter(curBook, curCh);
    else showChapter(BOOKS[0], 1);
}

// ── init ───────────────────────────────────────────────────────────────────
buildVerSelector();
buildSidebar();
showChapter(BOOKS[0], 1);
</script>
</body>
</html>
)html";

    return html.str();
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    string version   = "KJV";
    int    port      = 7778;
    bool   copyVerse = false;
    bool   csvMode   = false;
    bool   tabMode   = false;
    string planArg;
    string startArg;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if      (arg.find("-bv=") == 0)            version  = arg.substr(4);
        else if (arg.find("--bibleversion=") == 0) version  = arg.substr(15);
        else if (arg.find("--port=") == 0)         port     = stoi(arg.substr(7));
        else if (arg.find("--plan=") == 0)         planArg  = arg.substr(7);
        else if (arg.find("--start=") == 0)        startArg = arg.substr(8);
        else if (arg == "--verse" || arg == "-v")  copyVerse = true;
        else if (arg == "--csv"   || arg == "-c")  csvMode   = true;
        else if (arg == "--tab"   || arg == "-t")  tabMode   = true;
        else if (arg == "-h" || arg == "--help") {
            cout << "Usage: biblereader [-bv=KJV|BSB|WEB] [--port=7778] [--verse|-v] [--csv|-c] [--tab|-t] [--plan=NAME] [--start=DATE]\n"
                 << "  Opens a browser Bible reader.\n"
                 << "  Click a verse to copy its reference to the clipboard.\n"
                 << "  --verse,        -v   Copy verse text instead of reference\n"
                 << "  --csv,          -c   Print all days of the year as CSV (day,date,\"reference\")\n"
                 << "  --tab,          -t   Print all days of the year as TSV (day<TAB>date<TAB>reference)\n"
                 << "  --plan=NAME          Reading plan: Chronological (default), Sequential, OTNT\n"
                 << "  --start=DATE         Day 1 of the plan: mm/dd/yyyy, today, or day number\n"
                 << "  Press Enter here to quit; the last selection is printed to stdout.\n";
            return 0;
        }
    }

    if (csvMode || tabMode) {
#ifdef _WIN32
        char exeBuf[MAX_PATH] = {};
        GetModuleFileNameA(NULL, exeBuf, MAX_PATH);
        string exeDir = string(exeBuf);
        size_t sl = exeDir.find_last_of("\\/");
        if (sl != string::npos) exeDir = exeDir.substr(0, sl + 1); else exeDir = "";
        string bvPath = exeDir + "bv.exe";
        string baseBv;
        { ifstream t(bvPath); baseBv = t.good() ? "\"" + bvPath + "\"" : "bv"; }
#else
        ifstream localBv("./bv");
        string baseBv = localBv.good() ? "./bv" : "bv";
#endif
        string cmd = baseBv + (csvMode ? " --alldays --csv" : " --alldays --tab");
        if (!planArg.empty())  cmd += " --plan="  + planArg;
        if (!startArg.empty()) cmd += " --start=" + startArg;
        return system(cmd.c_str());
    }

    transform(version.begin(), version.end(), version.begin(), ::toupper);

    string bibleFile, bibleUrl;
    if      (version == "KJV") { bibleFile = "BibleKJV.txt"; bibleUrl = "https://raw.githubusercontent.com/codelife-us/VerseLumen/main/BibleKJV.txt"; }
    else if (version == "BSB") { bibleFile = "BibleBSB.txt"; bibleUrl = "https://bereanbible.com/bsb.txt"; }
    else if (version == "WEB") { bibleFile = "BibleWEB.txt"; bibleUrl = "https://raw.githubusercontent.com/codelife-us/VerseLumen/main/BibleWEB.txt"; }
    else { cerr << "Unknown version '" << version << "'. Use KJV, BSB, or WEB.\n"; return 1; }

    // Resolve file path
    {
        ifstream test(bibleFile);
        if (!test.good()) {
            const char* home = getenv(HOME_ENV);
            if (home) {
                string hp = string(home) + "/" + bibleFile;
                ifstream ht(hp);
                if (ht.good()) bibleFile = hp;
            }
        }
    }
    {
        ifstream test(bibleFile);
        if (!test.good()) {
            cerr << "Bible file '" << bibleFile << "' not found.\nDownload it now? (y/n): ";
            char ans; cin >> ans;
            if (ans == 'y' || ans == 'Y') {
                string tmpFile = bibleFile + ".tmp";
                string cmd = "curl -L --fail \"" + bibleUrl + "\" -o \"" + tmpFile + "\"";
                if (system(cmd.c_str()) != 0) {
                    remove(tmpFile.c_str());
                    cerr << "Download failed.\n"; return 1;
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
                    cerr << "Failed to save " << bibleFile << "\n"; return 1;
                }
            } else { return 1; }
        }
    }

    cerr << "Loading " << bibleFile << "..." << flush;
    loadBible(bibleFile);
    cerr << " " << gText.size() << " verses loaded.\n";

    if (gBooks.empty()) { cerr << "No verses loaded.\n"; return 1; }

    signal(SIGINT,  sigHandler);
    signal(SIGTERM, sigHandler);
    atexit(doCleanup);

    // Create temp dir
#ifdef _WIN32
    {
        char base[MAX_PATH];
        GetTempPathA(MAX_PATH, base);
        g_tempDir = string(base) + "biblereader_" + to_string(GetCurrentProcessId());
        if (_mkdir(g_tempDir.c_str()) != 0) { perror("mkdir"); return 1; }
    }
#else
    {
        char tmp[] = "/tmp/biblereader_XXXXXX";
        const char* td = mkdtemp(tmp);
        if (!td) { perror("mkdtemp"); return 1; }
        g_tempDir = td;
    }
#endif

    // Copy all available Bible files into temp dir for version switching
    struct VPair { const char* ver; const char* fname; };
    const VPair allVers[] = {
        {"KJV", "BibleKJV.txt"},
        {"BSB", "BibleBSB.txt"},
        {"WEB", "BibleWEB.txt"}
    };
    auto toAbsolute = [](const string& path) -> string {
#ifdef _WIN32
        if (path.size() >= 2 && path[1] == ':') return path;
        char buf[MAX_PATH];
        if (_getcwd(buf, sizeof(buf))) return string(buf) + "/" + path;
#else
        if (path.empty() || path[0] == '/') return path;
        char buf[4096];
        if (getcwd(buf, sizeof(buf))) return string(buf) + "/" + path;
#endif
        return path;
    };
    vector<string> available;
    for (const auto& vp : allVers) {
        string path;
        if (string(vp.ver) == version) {
            path = bibleFile;  // already resolved
        } else {
            { ifstream t(vp.fname); if (t.good()) path = vp.fname; }
            if (path.empty()) {
                const char* home = getenv(HOME_ENV);
                if (home) {
                    string hp = string(home) + "/" + vp.fname;
                    ifstream t(hp);
                    if (t.good()) path = hp;
                }
            }
        }
        if (path.empty()) continue;
        available.push_back(vp.ver);
        string dest = g_tempDir + "/" + vp.fname;
#ifdef _WIN32
        CopyFileA(toAbsolute(path).c_str(), dest.c_str(), FALSE);
#else
        symlink(toAbsolute(path).c_str(), dest.c_str());
#endif
    }

    cerr << "Generating page..." << flush;
    {
        string html    = makeHtml(version, copyVerse, available);
        string pyScript = makePythonServer(html);
        cerr << " " << html.size() << " bytes HTML, "
             << pyScript.size() << " bytes server.\n" << flush;
#ifdef _WIN32
        wstring wpath(g_tempDir.begin(), g_tempDir.end());
        wpath += L"\\server.py";
        FILE* fp = _wfopen(wpath.c_str(), L"wb");
        if (!fp) { cerr << "ERROR: cannot write server.py\n"; doCleanup(); return 1; }
        fwrite(pyScript.data(), 1, pyScript.size(), fp);
        fclose(fp);
#else
        ofstream f(g_tempDir + "/server.py");
        if (!f) { cerr << "ERROR: cannot write server.py\n"; doCleanup(); return 1; }
        f << pyScript;
#endif
    }

    // Launch Python server
#ifdef _WIN32
    {
        // _spawnlp _P_NOWAIT returns a process HANDLE (intptr_t), not a PID.
        // Try Python Launcher (py -3) first — avoids Microsoft Store alias that
        // breaks programmatic launch. Fall back to python / python3.
        string script  = g_tempDir + "\\server.py";
        string portStr = to_string(port);
        struct { const char* exe; bool dash3; } tries[] = {
            {"py",      true },
            {"python",  false},
            {"python3", false},
        };
        for (auto& t : tries) {
            intptr_t h = t.dash3
                ? _spawnlp(_P_NOWAIT, t.exe, t.exe, "-3", script.c_str(), portStr.c_str(), nullptr)
                : _spawnlp(_P_NOWAIT, t.exe, t.exe,       script.c_str(), portStr.c_str(), nullptr);
            if (h != -1) { g_serverHandle = (HANDLE)h; break; }
        }
        if (!g_serverHandle) {
            cerr << "ERROR: cannot start Python. Install Python from python.org.\n";
            doCleanup(); return 1;
        }
    }
#else
    g_serverPid = fork();
    if (g_serverPid == 0) {
        execlp("python3", "python3",
               (g_tempDir + "/server.py").c_str(),
               to_string(port).c_str(), nullptr);
        _exit(1);
    }
    if (g_serverPid < 0) { perror("fork"); doCleanup(); return 1; }
#endif

#ifdef _WIN32
    Sleep(400);
#else
    usleep(400000);
#endif

    string url = "http://localhost:" + to_string(port) + "/";
#ifdef _WIN32
    system(("start \"\" \"" + url + "\"").c_str());
#else
    system(("open \"" + url + "\"").c_str());
#endif
    cerr << "Opened " << url << "\n";
    cerr << "Press Enter to quit.\n";

    cin.get();

    // Print last selected reference (close before cleanup so Windows can delete the file)
    {
        string selFile = g_tempDir + "/selected.txt";
        ifstream sf(selFile);
        string ref;
        if (sf.good()) getline(sf, ref);
        sf.close();
        if (!ref.empty()) cout << ref << "\n";
    }

    doCleanup();
    return 0;
}
