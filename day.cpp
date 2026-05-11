// MIT License
// Copyright (c) 2026 Code Life
//
// day.cpp — Print the current day of the year (Jan 1 = 1)
// Build: g++ -std=c++11 -o day day.cpp
// Default: print day number only.
// -y/--youtube opens YouTube daily search.
// Query template loaded from .day in current dir or $HOME.
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <cstdlib>
#include <clocale>
#include <cstdio>
using namespace std;

static string captureCommand(const string& cmd) {
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return "";
    string result;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) result += buf;
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

static int dayOfYear() {
    time_t t = time(nullptr);
    return localtime(&t)->tm_yday + 1;
}

// Parse mm/dd/yyyy or mm/dd/yy → day of year; returns -1 on failure.
static int parseDateArg(const string& s) {
    int mm = 0, dd = 0, yyyy = 0;
    if (sscanf(s.c_str(), "%d/%d/%d", &mm, &dd, &yyyy) != 3) return -1;
    if (yyyy < 100) yyyy += 2000;
    struct tm t = {};
    t.tm_year = yyyy - 1900;
    t.tm_mon  = mm - 1;
    t.tm_mday = dd;
    t.tm_hour = 12;
    if (mktime(&t) == (time_t)-1) return -1;
    return t.tm_yday + 1;
}

static string urlEncode(const string& s) {
    string r;
    for (unsigned char c : s) {
        if (c == ' ')                                            r += '+';
        else if (isalnum(c) || c=='-'||c=='_'||c=='.'||c=='~') r += c;
        else { char buf[4]; snprintf(buf, sizeof(buf), "%%%02X", c); r += buf; }
    }
    return r;
}

// Read a single key=value from .verselumen in current dir, then $HOME.
// Lines starting with # are ignored.
static string loadVerselumenSetting(const string& key) {
    string paths[2] = {".verselumen", ""};
    const char* home = getenv("HOME");
    if (home) paths[1] = string(home) + "/.verselumen";

    for (const string& path : paths) {
        if (path.empty()) continue;
        ifstream f(path);
        if (!f.good()) continue;
        string line;
        while (getline(f, line)) {
            size_t s = line.find_first_not_of(" \t\r\n");
            if (s == string::npos || line[s] == '#') continue;
            if (line.substr(s, key.size()) != key) continue;
            size_t eq = line.find('=', s + key.size());
            if (eq == string::npos) continue;
            size_t vs = line.find_first_not_of(" \t", eq + 1);
            if (vs == string::npos) continue;
            size_t ve = line.find_last_not_of(" \t\r\n");
            return line.substr(vs, ve - vs + 1);
        }
    }
    return "";
}

static void openUrl(const string& url, bool useApp = false) {
    string browser = useApp ? loadVerselumenSetting("browser") : "";
#ifdef _WIN32
    if (!browser.empty())
        system(("start \"\" \"" + browser + "\" \"" + url + "\"").c_str());
    else
        system(("start \"\" \"" + url + "\"").c_str());
#elif defined(__APPLE__)
    if (!browser.empty())
        system(("open -a \"" + browser + "\" \"" + url + "\"").c_str());
    else if (useApp)
        system(("open -a \"Google Chrome\" \"" + url + "\"").c_str());
    else
        system(("open \"" + url + "\"").c_str());
#else
    if (!browser.empty())
        system((browser + " \"" + url + "\" 2>/dev/null &").c_str());
    else
        system(("xdg-open \"" + url + "\"").c_str());
#endif
}

// Read default query from .day file in current dir, then $HOME.
// Skips blank lines and lines starting with #.
static string loadQueryFile() {
    string paths[2] = {".day", ""};
    const char* home = getenv("HOME");
    if (home) paths[1] = string(home) + "/.day";

    for (const string& path : paths) {
        if (path.empty()) continue;
        ifstream f(path);
        if (!f.good()) continue;
        string line;
        while (getline(f, line)) {
            size_t s = line.find_first_not_of(" \t\r\n");
            if (s == string::npos || line[s] == '#') continue;
            size_t e = line.find_last_not_of(" \t\r\n");
            return line.substr(s, e - s + 1);
        }
    }
    return "";
}

int main(int argc, char* argv[]) {
    bool   openYoutube = false;
    bool   useApp      = false;
    bool   planMode    = false;
    bool   refOnly     = false;
    bool   csvMode     = false;
    bool   tabMode     = false;
    int    dayOverride = -1;   // -1 = use current day
    string queryTpl;           // empty = load from .day file or use built-in default

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-v" || arg == "--version") {
            cout << "day v1.1\n";
            return 0;
        } else if (arg == "-h" || arg == "--help") {
            cout << "day — print the current day of the year (Jan 1 = 1)\n\n"
                 << "Usage: day [-d[=N]|--day[=N]] [-y|--youtube] [-q=TEXT|--query=TEXT] [-p|--plan] [-r|--refonly] [-c|--csv] [-t|--tab]\n\n"
                 << "  (default)              Print day number only\n"
                 << "  -d=N, --day=N          Use day N instead of today\n"
                 << "  -d=mm/dd/yyyy          Use date instead of day number (4-digit or 2-digit year)\n"
                 << "  -y, --youtube          Open YouTube daily search\n"
                 << "  -a, --app              Open YouTube in browser set by .verselumen browser=; implies -y\n"
                 << "                         macOS default (no browser= set): Google Chrome\n"
                 << "  -q=TEXT, --query=TEXT  Override search query ({day} = day number); implies -y\n"
                 << "  -p, --plan             Print day number, date, and Bible reference\n"
                 << "  -r, --refonly          Print Bible reference only\n"
                 << "  -c, --csv              Output as CSV: day,date,\"reference\"\n"
                 << "  -t, --tab              Output as TSV: day<TAB>date<TAB>reference\n"
                 << "  -v, --version          Print version\n"
                 << "  -h, --help             Show this help\n\n"
                 << "Config files (current dir or $HOME):\n"
                 << "  .day          First non-blank, non-comment line is the default search query.\n"
                 << "                Example:  Day {day} The Bible Recap\n"
                 << "  .verselumen   Key=value settings. Supported keys:\n"
                 << "                  browser=Firefox     # browser for -a (macOS default: Google Chrome)\n"
                 << "                  macOS: app name for open -a; Windows: executable; Linux: command\n\n"
                 << "Examples:\n"
                 << "  day                               # print day number\n"
                 << "  day -r                            # print today's Bible reference\n"
                 << "  day -d=3/21/2026 -r               # Bible reference for a specific date\n"
                 << "  day -y                            # open YouTube for today's recap\n"
                 << "  day -d=203 -y                     # open YouTube for day 203\n"
                 << "  day -q=\"Day {day} The Bible Recap\" # custom query, opens YouTube\n"
                 << "  day -p                            # print day number, date, and reference\n";
            return 0;
        } else if (arg.find("-d=") == 0 || arg.find("--day=") == 0) {
            string val = arg.substr(arg.find('=') + 1);
            dayOverride = (val.find('/') != string::npos) ? parseDateArg(val) : stoi(val);
        } else if (arg == "-d" || arg == "--day") {
            // no-op: day-only is now the default
        } else if (arg == "--youtube" || arg == "-y") {
            openYoutube = true;
        } else if (arg == "--app" || arg == "-a") {
            openYoutube = true;
            useApp = true;
        } else if (arg.find("--query=") == 0) {
            queryTpl = arg.substr(8);
            openYoutube = true;
        } else if (arg.find("-q=") == 0) {
            queryTpl = arg.substr(3);
            openYoutube = true;
        } else if (arg == "-p" || arg == "--plan") {
            planMode = true;
        } else if (arg == "-r" || arg == "--refonly") {
            refOnly = true;
        } else if (arg == "-c" || arg == "--csv") {
            csvMode = true;
        } else if (arg == "-t" || arg == "--tab") {
            tabMode = true;
        }
    }

    int day = (dayOverride > 0) ? dayOverride : dayOfYear();

    if (refOnly) {
        ifstream localBv("./bv");
        string baseBv = localBv.good() ? "./bv" : "bv";
        return system((baseBv + " --day=" + to_string(day) + " --refonly").c_str());
    }

    if (!csvMode && !tabMode) cout << day << "\n";

    if (planMode || csvMode || tabMode) {
        setlocale(LC_TIME, "");
        time_t now = time(nullptr);
        struct tm target = *localtime(&now);
        target.tm_mon  = 0;
        target.tm_mday = day;  // mktime normalizes day-of-year into correct month/day
        target.tm_hour = 12;
        target.tm_min  = 0;
        target.tm_sec  = 0;
        mktime(&target);
        char dateBuf[64];
        strftime(dateBuf, sizeof(dateBuf), "%m/%d/%Y", &target);

        ifstream localBv("./bv");
        string baseBv = localBv.good() ? "./bv" : "bv";

        if (csvMode || tabMode) {
            string ref = captureCommand(baseBv + " --day=" + to_string(day) + " --refonly");
            if (csvMode)
                cout << day << "," << dateBuf << ",\"" << ref << "\"\n";
            else
                cout << day << "\t" << dateBuf << "\t" << ref << "\n";
        } else {
            cout << dateBuf << "\n";
            cout.flush();
            system((baseBv + " --day=" + to_string(day) + " --refonly").c_str());
        }
    }
    if (openYoutube) {
        if (queryTpl.empty()) {
            queryTpl = loadQueryFile();
            if (queryTpl.empty()) queryTpl = "Day {day} The Bible Recap";
        }

        string q = queryTpl;
        string dayStr = to_string(day);
        size_t pos;
        while ((pos = q.find("{day}")) != string::npos)
            q.replace(pos, 5, dayStr);

        openUrl("https://www.youtube.com/results?search_query=" + urlEncode(q), useApp);
    }
}
