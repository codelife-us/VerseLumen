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
// gospelshare.cpp
// This program outputs various gospel presentation tracts, biblical frameworks for sharing the Gospel of Jesus Christ.
// Also there is the --ref option to output only Bible references from one or many references provided.

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <regex>
#include <fstream>
#include <sstream>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#define popen  _popen
#define pclose _pclose
#else
#include <termios.h>
#include <unistd.h>
#endif

using namespace std;

#ifdef _WIN32
#define HOME_ENV "USERPROFILE"
#else
#define HOME_ENV "HOME"
#endif

const string VERSION = "1.3";
const string CONFIG_FILE = ".verselumen";
const string SECTION     = "gospelshare";

// ── Config file (.verselumen, [gospelshare] section; also $HOME/.verselumen) ───────────

// Read key=value pairs from the [gospelshare] section of CONFIG_FILE.
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

struct Section {
    string section_title;
    string text;
};

struct Tract {
    string name;
    vector<Section> sections;
};

// Romans Road sections
vector<Section> romansRoadSections = {
    {"What is The Romans Road?", "\nSimply put, the Romans Road shows the path to Heaven. It uses points from the book of Romans in the Bible to explain God’s gift of salvation. At each stop we learn something new about why we need salvation, how God has provided for it, and how we receive it.\n"
"\nAs you look at each of the six points outlined here, take time to reflect on what God’s Word has to say. Remember that the book of Romans was written to people just like you and me: people living in a busy culture, trying to make sense of what is true and what is most important in life.\n"
"\nThe Romans Road describes the only way to Heaven. Study it, learn it, and follow it!"},
    {"1. Everyone Needs Salvation", "\n[Romans 3:23]"},
    {"2. Jesus Died For Our Salvation", "\n[Romans 5:8]"},
    {"3. Salvation Is A Gift", "\n[Romans 6:23]"},
    {"4. We Are Saved By Grace", "\n[Romans 11:6]"},
    {"5. Salvation Comes Through Faith", "\n[Romans 4:5]"},
    {"6. God Saves All Who Call Upon Him", "\n[Romans 10:13]\n\nThe Bible says there are two roads in life: one is the way to eternal joy in Heaven, and the other leads to eternal punishment in Hell. Which road are you on?\n\nFollow the Romans Road to salvation today. Recognize that you are a sinner and that your sin must be judged by God. See that Jesus died to pay the penalty for your sin, but that you must choose to accept His provision. Understand that you cannot earn your way to Heaven through good works or religious activity. Now turn to God and put your faith in Jesus Christ who died for you and rose again.\n\n[Romans 10:9-10]"},
};

vector<Section> tract2 = {
    {"<span style=\"color: red;\">Somebody Loves You</span>", "\nThe creator of all things loves <span style=\"color: #0c9c30;\">YOU</span> so much that He sent His only Son, Jesus, to die for you on the cross. He wants to forgive your sins and make you clean so that you can be with Him for all eternity. Isn’t it amazing to know that you are loved by God?\n\n**HIS LOVE FOR YOU IS:...**"},
    {"<span style=\"color: red;\">Unending</span>", "— God’s love is eternal. He loved you yesterday, loves you today and will love you forever!\n\n[Jeremiah 31:3]"},
    {"<span style=\"color: red;\">Unselfish</span>", "— God didn’t wait for us to love Him first. His love was freely given.\n\n[1 John 4:19]\n"},
    {"<span style=\"color: red;\">Undeserved</span>", "— God is holy, righteous, and just. As sinners, we have done nothing to deserve God’s love. But He loves us anyway.\n\n[Romans 5:8]\n"},
    {"<span style=\"color: red;\">Unimaginable</span>", "— God’s love led Him to send His son Jesus to bear the punishment for our sins on the cross. Can you imagine watching someone you love being punished to the point of death for something they didn’t do?\n\n[John 3:16]\n"},
    {"<span style=\"color: red;\">Undying</span>", "— God didn’t only send Jesus to die for our sins. He then raised Him from the dead so we can have eternal life. What a wonderful gift He offers us!\n\n[1 John 4:9]\n"},
    {"<span style=\"color: red;\">Unmerited</span>", "— God offers you the free gift of salvation through Jesus Christ. Because it is a gift, you cannot earn it. Instead, you must see your need of salvation and accept it by trusting Jesus as your Savior.\n\n[Ephesians 2:8-9]\n\nHow can you receive God’s gift of salvation? The Bible says:\n\n[Romans 10:9]\n\nIt’s that easy! Will you choose to accept God’s love? Then you too can experience God’s unfathomable love for all eternity!\n\n[Romans 8:38-39]\n"}
};
vector<Section> tract3 = {
    {"Have a good day!","Have you ever stopped to think about how many times a day this phrase is used? Whether we’re at a store, an appointment, or even just talking to a neighbor next door, we usually hear...\n\n<center>**Have a good day!**</center>\n\nNow, just receiving this wish doesn’t guarantee our day will go smoothly. Life happens and four words can’t erase all of the difficult things we face. We’re left to ask ourselves, how can we ever truly have a good day—not only today, but every day?"},
    {"A Good Day and a Good Forever","It’s hard to have a good day when we’re unsure of the future. How will I pay that bill? What will the test results show? Looking further ahead, uncertainty about death and what comes after it can also weigh on our minds. So often, we distract ourselves with things in our lives that make us happy and keep us busy.\n\nBut have you seriously considered what your future holds? Do you know where you will spend eternity? There are only two choices—Heaven or Hell. And only one of those guarantees you a good forever."},
    {"One Little Word","There is one thing that determines where we will spend our forever—our sin. Anything we say, do, or think that goes against what God wants is considered sin and separates us from Him. The Bible says,\n[Romans 3:23]\nSin will keep you from having a good forever, and it often keeps you from having a good day. God, who is holy, must punish sin. We read that\n[Romans 6:23]\nSin also leads to judgment:\n[Hebrews 9:27]\nDefinitely not a good day!"},
    {"God Provided the Way","But there is good news! Even though we are sinners, God loves us so much He has provided the way for us to spend eternity in Heaven with Him. The Bible says, [Romans 5:8]"},
    {"The Source of Good Days","Will you accept His free gift of salvation? Admit you are a sinner, believe that Jesus died for your sins, and confess that He is your Lord and Savior today. Then as a believer, you will not only be sure of a good forever, you will also be able to have a good day every day. Jesus will never leave you, and the more you fellowship with Him through prayer and reading the Bible daily, the more you will experience His presence in your life. Even if everything around you seems dark and discouraging, walking with God will let you say: [Psalm 118:24]"}
};

vector<Section> tract4 = {
    {"You must meet God. Are you ready?","\nIn comparison with this question, all others are utterly insignificant. You may be successful in business, have a wonderful family, and be healthy in both body and mind. You may even be religious and respected by all. But can you say with certainty that **you are ready to meet God?**\n"},
    {"","To be ready to meet God requires a new birth.\n[John 3:3]\n\nThis statement has no exceptions. It cannot be ignored, as it was given by the Lord Jesus Christ. Have you been born again?\n"
        "\nThe need for new birth arises from the fact that we are **all** sinners by birth and by practice. Sin is contrary to the nature of God. He hates sin, and it keeps us from having fellowship with Him. The Bible declares:\n[Habakkuk 1:13]\n"
        "\nBut while God hates sin, He **loves** sinners. In fact, He loves us whether we walk the clean or filthy side of the road to Hell. Because He loves us, He has provided the way for us to be restored to fellowship with Him:\n[John 3:16]\n"
        "\nGod’s judgment for sin **fell upon Christ** on the cross, and there is cleansing and forgiveness for all who **believe on Him.**\n[John 1:12]\nThrough new birth, by receiving Christ, by believing He died for my sins, I am born into the family of God and can answer the question **“Are you ready?”** with a glad and confident “Yes!”\n"
        "\nHave you known this change? Have you experienced this new birth? Have you turned to God in repentance, admitting that you are a sinner in need of salvation? Have you given up the false idea that you can earn God’s favor through your own “good” works? Have you by faith accepted Jesus Christ as your Savior and Lord?\n"
        "\nDo not resent these personal questions, for they are intended to lead you to **blessing.** If I were to tell you of a path to wealth and fame you would probably be very interested. What Jesus Christ offers you—forgiveness of sins and eternal life with Him in Heaven—is of **infinite** value. Put the matter to the test. Do it now.\n"
        "\nThe terms are simple:\n[1 John 3:23]\n\nWill you receive Him, the living Christ of God who died, rose again, and now offers salvation to all who believe on Him? Will you trust Him **now** as your very own Savior? It is not the reception of a creed, or identification with a church, or becoming a religious person that saves. It is the acceptance of a Person, the Son of God. Your acceptance or rejection of Him answers the solemn question, “Are you ready?”\n"
        "\n[Hebrews 9:27]\nWhen you stand before God, will you be **stained** with your sin or will you have been washed **clean** through faith in Jesus Christ? The issue is very clear: your response to Jesus Christ **now** will determine your destiny for **eternity**.\n\n[Acts 4:12]\n[Acts 16:31]\n"},
    {"You must meet God. Are you ready?",""}
};

// Available tracts
map<string, Tract> availableTracts = {
    {"The Romans Road", {"The Romans Road", romansRoadSections}},
    {"Somebody Loves You", {"Somebody Loves You", tract2}},
    {"Have A Good Day", {"Have A Good Day", tract3}},
    {"Are You Ready", {"Are You Ready", tract4}}
};

struct TractDefaults {
    int refStyle;
    int verseQuotes;
    TractDefaults(int rs = -1, int vq = -1) : refStyle(rs), verseQuotes(vq) {}
};

map<string, TractDefaults> tractDefaults = {
    {"Are You Ready", {3, 1}}
};

// Function to load Bible verses from file
map<string, string> loadBible(const string& filename) {
    map<string, string> verses;
    ifstream file(filename);
    string line;
    while (getline(file, line)) {
        // Remove BOM if present
        if (line.size() >= 3 &&
            (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB &&
            (unsigned char)line[2] == 0xBF) {
            line = line.substr(3);
        }
        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // Parse any line that looks like "Book Chapter:Verse\tText"
        size_t tab = line.find('\t');
        if (tab == string::npos) continue;
        string ref = line.substr(0, tab);
        // Ref must contain a colon to be a valid verse reference
        if (ref.find(':') == string::npos) continue;
        string text = line.substr(tab + 1);
        verses[ref] = text;
    }
    return verses;
}

// Bible verse database
map<string, string> bibleVerses;

// Look up one or more verses for a reference like "Romans 8:9" or "Romans 8:9-10"
// Returns the verse text, or empty string on failure. Writes errors to stderr.
string lookupVerses(const string& reference, bool verseNumbers = false, bool verseNewline = false, bool markdown = false) {
    // Check for a verse range (e.g. "Romans 8:9-10" or "Romans 8:20-")
    size_t colon = reference.rfind(':');
    if (colon != string::npos) {
        size_t dash = reference.find('-', colon);
        if (dash != string::npos) {
            string bookChapter = reference.substr(0, colon + 1); // "Romans 8:"
            int startVerse = stoi(reference.substr(colon + 1, dash - colon - 1));

            // Determine end verse: find last verse in chapter if nothing follows the dash
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
                    cerr << "Warning: no verses found for chapter in '" << reference << "'." << endl;
                    return "";
                }
            } else {
                endVerse = stoi(afterDash);
            }

            string combined;
            bool anyMissing = false;
            for (int v = startVerse; v <= endVerse; ++v) {
                string key = bookChapter + to_string(v);
                auto it = bibleVerses.find(key);
                if (it != bibleVerses.end()) {
                    if (!combined.empty()) combined += verseNewline ? (markdown ? "  \n" : "\n") : " ";
                    if (verseNumbers) combined += "[" + to_string(v) + "] ";
                    combined += it->second;
                } else {
                    cerr << "Warning: verse not found: " << key << endl;
                    anyMissing = true;
                }
            }
            if (combined.empty()) return "";
            if (anyMissing) {
                cerr << "Warning: some verses in range '" << reference << "' were not found." << endl;
            }
            return combined;
        }
    }
    // No colon: treat as entire chapter (e.g. "Romans 8")
    if (colon == string::npos) {
        string prefix = reference + ":";
        // Collect matching verses, keyed by verse number for correct ordering
        map<int, string> chapterVerses;
        for (const auto& entry : bibleVerses) {
            if (entry.first.compare(0, prefix.size(), prefix) == 0) {
                int verseNum = stoi(entry.first.substr(prefix.size()));
                chapterVerses[verseNum] = entry.second;
            }
        }
        if (chapterVerses.empty()) {
            cerr << "Warning: no verses found for chapter '" << reference << "'." << endl;
            return "";
        }
        string combined;
        for (const auto& v : chapterVerses) {
            if (!combined.empty()) combined += verseNewline ? (markdown ? "  \n" : "\n") : " ";
            if (verseNumbers) combined += "[" + to_string(v.first) + "] ";
            combined += v.second;
        }
        return combined;
    }

    // Single verse lookup — verse number comes from the reference itself
    auto it = bibleVerses.find(reference);
    if (it != bibleVerses.end()) {
        if (verseNumbers) {
            int verseNum = stoi(reference.substr(colon + 1));
            return "[" + to_string(verseNum) + "] " + it->second;
        }
        return it->second;
    }
    return "";
}

string formatCitation(const string& verseText, const string& reference, const string& version, bool markdown, int refStyle, bool italic = false, bool verseQuotes = false) {
    string citation = reference + " (" + version + ")";
    string quotedText = verseQuotes ? "\u201c" + verseText + "\u201d" : verseText;
    string formatted;
    if (refStyle == 2) {
        formatted = quotedText + " - " + citation;
    } else if (refStyle == 3) {
        formatted = quotedText + " (" + reference + ")";
    } else if (refStyle == 4) {
        formatted = quotedText + " (" + citation + ")";
    } else {
        formatted = quotedText + "  \n\u2014 " + citation;
    }
    if (markdown && italic) {
        formatted = "*" + formatted + "*";
    }
    return formatted;
}

// Registry of hex colors -> auto-generated LaTeX color names.
// Populated during text processing; written as \definecolor into the PDF header.
map<string, string> pdfColorRegistry;
int pdfColorCounter = 0;

string registerPdfColor(const string& hex) {
    auto it = pdfColorRegistry.find(hex);
    if (it != pdfColorRegistry.end()) return it->second;
    string name = "gospelsharecolor" + to_string(pdfColorCounter++);
    pdfColorRegistry[hex] = name;
    return name;
}

// Wrap text in \textcolor{name}{text} for PDF output.
// Hex colors are registered for \definecolor in the header; named colors used directly.
string applyPdfColor(const string& text, const string& color) {
    if (color.empty()) return text;
    string c = color;
    if (!c.empty() && c[0] == '#') c = c.substr(1);
    bool isHex = c.size() == 6 && c.find_first_not_of("0123456789abcdefABCDEF") == string::npos;
    if (isHex) {
        transform(c.begin(), c.end(), c.begin(), ::toupper);
        return "\\textcolor{" + registerPdfColor(c) + "}{" + text + "}";
    }
    return "\\textcolor{" + color + "}{" + text + "}";
}

// Convert <span style="color: #hex;">text</span> to \textcolor[HTML]{HEX}{text} for PDF output.
// Supports hex colors (#RRGGBB) and named colors (red, blue, etc.).
string convertSpansToPdfColor(const string& text) {
    string result;
    regex spanPattern("<span\\s+style=[\"']color:\\s*([^\"';]+);?\\s*[\"']>(.*?)</span>",
                      regex::icase);
    sregex_iterator it(text.cbegin(), text.cend(), spanPattern);
    sregex_iterator end;
    size_t lastEnd = 0;

    for (; it != end; ++it) {
        const smatch& m = *it;
        result += text.substr(lastEnd, m.position() - lastEnd);
        string color = m[1].str();
        size_t s = color.find_first_not_of(" \t");
        size_t e = color.find_last_not_of(" \t");
        if (s != string::npos) color = color.substr(s, e - s + 1);
        result += applyPdfColor(m[2].str(), color);
        lastEnd = m.position() + m.length();
    }
    result += text.substr(lastEnd);
    return result;
}

// Convert <center>...</center> to \begin{center}...\end{center} for PDF output.
// Also converts **bold** to \textbf{} inside center blocks, since pandoc won't
// process markdown inside raw LaTeX blocks.
string convertCenteringToPdf(const string& text) {
    regex centerPattern("<center>([\\s\\S]*?)</center>", regex::icase);
    sregex_iterator it(text.cbegin(), text.cend(), centerPattern);
    sregex_iterator end;
    string result;
    size_t lastEnd = 0;

    for (; it != end; ++it) {
        const smatch& m = *it;
        result += text.substr(lastEnd, m.position() - lastEnd);
        string content = m[1].str();
        content = regex_replace(content, regex("\\*\\*([^*]+)\\*\\*"), "\\textbf{$1}");
        result += "\\begin{center}" + content + "\\end{center}";
        lastEnd = m.position() + m.length();
    }
    result += text.substr(lastEnd);
    return result;
}

string convertForPdf(const string& text) {
    return convertCenteringToPdf(convertSpansToPdfColor(text));
}

// Strip <center>...</center> tags for markdown output, leaving only the content.
string stripCenterTags(const string& text) {
    regex centerPattern("<center>([\\s\\S]*?)</center>", regex::icase);
    return regex_replace(text, centerPattern, "$1");
}

// Strip <span ...>...</span> tags for markdown output, leaving only the content.
string stripSpanTags(const string& text) {
    regex spanPattern("<span[^>]*>(.*?)</span>", regex::icase);
    return regex_replace(text, spanPattern, "$1");
}

// Strip **bold** markers for plain text output, leaving only the content.
string stripBoldMarkers(const string& text) {
    return regex_replace(text, regex("\\*\\*([^*]+)\\*\\*"), "$1");
}

// Process markdown text with embedded bible references
string processMarkdownReferences(const string& text, const string& version, bool markdown, int refStyle, bool verseNumbers, bool verseQuotes = false, bool isPdf = false, bool verseNewline = false) {
    string result = isPdf ? convertForPdf(text) : (markdown ? stripSpanTags(stripCenterTags(text)) : stripBoldMarkers(text));
    regex refPattern("\\[([^\\]]+)\\]");
    smatch match;
    string::const_iterator searchStart(text.cbegin());
    size_t offset = 0;

    while (regex_search(searchStart, text.cend(), match, refPattern)) {
        string reference = match[1].str();

        string verseText = lookupVerses(reference, verseNumbers, verseNewline, markdown);
        if (!verseText.empty()) {
            string replacement = formatCitation(verseText, reference, version, markdown, refStyle, true, verseQuotes);

            // Replace the [Reference] with the verse text and formatted reference
            size_t pos = result.find("[" + reference + "]", offset);
            if (pos != string::npos) {
                result.replace(pos, reference.length() + 2, replacement);
                offset = pos + replacement.length();
            }
        }

        searchStart = match.suffix().first;
    }

    return result;
}

// Interactive arrow-key tract picker. Returns the selected tract name, or ""
// if the user cancels with 'q' or Ctrl+C.
string tractPick() {
    vector<string> names;
    for (const auto& t : availableTracts) names.push_back(t.first);
    sort(names.begin(), names.end());

#ifdef _WIN32
    // Windows: use _getch for raw key input
    int sel = 0;
    int n = (int)names.size();
    auto clearLines = [&](int count) {
        for (int i = 0; i < count; ++i) {
            cout << "\033[A\033[2K";
        }
    };
    auto draw = [&]() {
        cout << "Select a tract (arrow keys + Enter, q to cancel):\n";
        for (int i = 0; i < n; ++i) {
            cout << (i == sel ? "  > " : "    ") << names[i] << "\n";
        }
        cout.flush();
    };
    draw();
    while (true) {
        int ch = _getch();
        if (ch == 0 || ch == 0xE0) {
            int ch2 = _getch();
            clearLines(n + 1);
            if (ch2 == 72 && sel > 0) --sel;       // up
            else if (ch2 == 80 && sel < n-1) ++sel; // down
            draw();
        } else if (ch == '\r' || ch == '\n') {
            clearLines(n + 1);
            return names[sel];
        } else if (ch == 'q' || ch == 'Q' || ch == 3) {
            clearLines(n + 1);
            return "";
        }
    }
#else
    // Unix: use termios raw mode
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    int sel = 0;
    int n = (int)names.size();
    auto clearLines = [&](int count) {
        for (int i = 0; i < count; ++i) cout << "\033[A\033[2K";
    };
    auto draw = [&]() {
        cout << "Select a tract (arrow keys + Enter, q to cancel):\n";
        for (int i = 0; i < n; ++i)
            cout << (i == sel ? "  > " : "    ") << names[i] << "\n";
        cout.flush();
    };
    draw();
    string result;
    while (true) {
        int ch = getchar();
        if (ch == 27) {                     // escape sequence
            int ch2 = getchar();
            if (ch2 == '[') {
                int ch3 = getchar();
                clearLines(n + 1);
                if (ch3 == 'A' && sel > 0) --sel;       // up arrow
                else if (ch3 == 'B' && sel < n-1) ++sel; // down arrow
                draw();
            }
        } else if (ch == '\n' || ch == '\r') {
            clearLines(n + 1);
            result = names[sel];
            break;
        } else if (ch == 'q' || ch == 'Q' || ch == 3) {
            clearLines(n + 1);
            break;
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return result;
#endif
}

string generateTractContent(const Tract& tract, const string& version,
                            bool markdown, bool isPdf, int refStyle,
                            bool verseNumbers, bool verseQuotes, bool verseNewline) {
    ostringstream out;
    for (const auto& v : tract.sections) {
        if (v.section_title.length() > 0) {
            bool textOnSameLine = v.text.length() > 0 && v.text[0] != '\n';
            if (markdown) {
                string title = isPdf ? convertForPdf(v.section_title) : stripSpanTags(stripCenterTags(v.section_title));
                if (textOnSameLine) {
                    out << "\n**" << title << "**";
                } else {
                    out << "\n## " << title;
                }
            } else {
                out << v.section_title;
            }
            if (v.text.length() > 0) {
                string processedText = processMarkdownReferences(v.text, version, markdown, refStyle, verseNumbers, verseQuotes, isPdf, verseNewline);
                if (markdown) {
                    string hardBreak;
                    for (size_t i = 0; i < processedText.size(); ++i) {
                        if (processedText[i] == '\n' && (i < 2 || processedText[i-1] != ' ' || processedText[i-2] != ' ')) {
                            hardBreak += "  \n";
                        } else {
                            hardBreak += processedText[i];
                        }
                    }
                    processedText = hardBreak;
                }
                out << " " << processedText << "\n" << endl;
            } else {
                out << "\n" << endl;
            }
        } else if (v.text.length() > 0) {
            string processedText = processMarkdownReferences(v.text, version, markdown, refStyle, verseNumbers, verseQuotes, isPdf, verseNewline);
            out << processedText << "\n" << endl;
        }
    }
    return out.str();
}

bool writePdfFile(const string& content, const string& outputFile,
                  const string& pdfMargin, const string& pdfFont, int pdfFontSizePct) {
    string tmpFile = outputFile + ".tmp.md";
    ofstream tmp(tmpFile, ios::binary);
    if (!tmp) {
        cerr << "Error: could not create temporary file '" << tmpFile << "'." << endl;
        return false;
    }
    tmp.write("\xEF\xBB\xBF", 3);
    tmp << content;
    tmp.close();

    string cmd = "pandoc -f markdown+raw_tex -V geometry:margin=" + pdfMargin;
    string headerFile = outputFile + ".tmp.tex";
    double scale = pdfFontSizePct / 100.0;
    bool needHeader = !pdfFont.empty() || !pdfColorRegistry.empty();
    if (needHeader) {
        if (!pdfFont.empty()) cmd += " --pdf-engine=xelatex";
        ofstream hdr(headerFile);
        if (!hdr) {
            cerr << "Error: could not create header file '" << headerFile << "'." << endl;
            remove(tmpFile.c_str());
            return false;
        }
        if (!pdfFont.empty()) {
            hdr << "\\usepackage{fontspec}\n";
            hdr << "\\setmainfont[Scale=" << scale << "]{" << pdfFont << "}\n";
        }
        if (!pdfColorRegistry.empty()) {
            hdr << "\\usepackage{xcolor}\n";
            for (const auto& entry : pdfColorRegistry)
                hdr << "\\definecolor{" << entry.second << "}{HTML}{" << entry.first << "}\n";
        }
        hdr.close();
        cmd += " -H \"" + headerFile + "\"";
    }
    cmd += " \"" + tmpFile + "\" -o \"" + outputFile + "\"";
    int ret = system(cmd.c_str());
    remove(tmpFile.c_str());
    remove(headerFile.c_str());
    if (ret != 0) {
        cerr << "Error: PDF conversion failed for '" << outputFile << "'." << endl;
        return false;
    }
    cerr << "Saved " << outputFile << endl;
    return true;
}

string tractSlug(const string& name) {
    string s = name;
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    s.erase(remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

bool writeEpubFile(const string& content, const string& outputFile, const string& coverImage = "", const string& title = "") {
    string tmpFile = outputFile + ".tmp.md";
    string qrTmpFile = outputFile + ".tmp.qr.png";
    bool hasQr = false;

    ofstream tmp(tmpFile, ios::binary);
    if (!tmp) {
        cerr << "Error: could not create temporary file '" << tmpFile << "'." << endl;
        return false;
    }
    tmp.write("\xEF\xBB\xBF", 3);
    tmp << content;

    // Append gospelshare_epub_add.txt if present
    ifstream appendIn("gospelshare_epub_add.txt");
    if (appendIn.good()) {
        string appendContent((istreambuf_iterator<char>(appendIn)), istreambuf_iterator<char>());
        appendIn.close();

        // Find last non-empty line
        string lastLine;
        istringstream iss(appendContent);
        string line;
        while (getline(iss, line)) {
            size_t s = line.find_first_not_of(" \t\r");
            if (s != string::npos) lastLine = line.substr(s);
            // trim trailing whitespace
            size_t e = lastLine.find_last_not_of(" \t\r");
            if (e != string::npos) lastLine = lastLine.substr(0, e + 1);
        }

        tmp << "\n\n---\n\n" << appendContent;

        if (lastLine.find("http://") == 0 || lastLine.find("https://") == 0) {
            string qrCmd = "qrencode -s 6 -o \"" + qrTmpFile + "\" \"" + lastLine + "\"";
            if (system(qrCmd.c_str()) == 0) {
                hasQr = true;
                tmp << "\n\n![](" << qrTmpFile << ")\n";
            } else {
                cerr << "Warning: qrencode not available; skipping QR code." << endl;
                cerr << "  macOS:  brew install qrencode" << endl;
                cerr << "  Linux:  apt install qrencode" << endl;
            }
        }
    }
    tmp.close();

    string cmd = "pandoc -f markdown --epub-title-page=false -o \"" + outputFile + "\"";
    if (!title.empty())
        cmd += " --metadata title=\"" + title + "\"";
    if (!coverImage.empty()) {
        ifstream test(coverImage);
        if (test.good()) {
            cmd += " --epub-cover-image=\"" + coverImage + "\"";
        } else {
            cerr << "Warning: cover image not found: " << coverImage << endl;
        }
    }
    cmd += " \"" + tmpFile + "\"";

    int ret = system(cmd.c_str());
    remove(tmpFile.c_str());
    if (hasQr) remove(qrTmpFile.c_str());
    if (ret != 0) {
        cerr << "Error: EPUB conversion failed for '" << outputFile << "'." << endl;
        return false;
    }
    cerr << "Saved " << outputFile << endl;
    return true;
}

void printHelp() {
    cout << "gospelshare v" << VERSION << endl;
    cout << "\nUsage: gospelshare [OPTIONS]" << endl;
    cout << "\nOptions:" << endl;
    cout << "  -h, --help              Show this help message and exit" << endl;
    cout << "  -v, --version           Show version information and exit" << endl;
    cout << "  -bv=VERSION             Set Bible version (default: KJV)" << endl;
    cout << "  --bibleversion=VERSION  Specify Bible version (KJV, BSB, WEB)" << endl;
    cout << "  --outputtype=TYPE       Set output type (plaintext, md)" << endl;
    cout << "  -tn=NAME                Set tract name (default: 'The Romans Road')" << endl;
    cout << "  --tractlist, -tl        List available tract names and exit" << endl;
    cout << "  --tractpick, -tp        Interactively pick a tract by name with arrow keys" << endl;
    cout << "  --tractname=NAME        Specify tract presentation by name" << endl;
    cout << "  --ref=REF                Output a Bible reference directly (use comma to separate multiple)" << endl;
    cout << "                           REF formats: Book Ch:V  Book Ch:V-V  Book Ch:V-  Book Ch" << endl;
    cout << "  --refstyle=STYLE         Citation style: 1=new line (default), 2=inline, 3=parentheses, 4=parentheses with version" << endl;
    cout << "  --versenumbers, -vn      Prefix each verse with its verse number, e.g. [1]" << endl;
    cout << "  --versenewline, -vnl     Start each verse on a new line" << endl;
    cout << "  --output=FILE            Write output to FILE (.pdf/.epub require pandoc)" << endl;
    cout << "  --pdfmargin=MARGIN       PDF margin size (default: 0.5in, e.g. 0.75in, 2cm)" << endl;
    cout << "  --pdffont=FONT           PDF font name (default: Palatino, requires xelatex)" << endl;
    cout << "  --pdffontsize=PCT        PDF font size as a percentage (default: 100, e.g. 120 = 120%)" << endl;
    cout << "  --italic                 Italicize verse output (default: off for --ref, on for tracts)" << endl;
    cout << "  --versequotes            Wrap each Bible verse in curly quotes" << endl;
    cout << "  --chapterheader, -ch     Print book and chapter as a header when outputting a full chapter" << endl;
    cout << "  --print                  Send PDF to printer after generating (requires --output=.pdf)" << endl;
    cout << "  --outputall              Output .txt, .md, .pdf, .epub for every tract and Bible version" << endl;
    cout << "  --titlegraphic           Use cover image for .epub; default: {tractname}_1.jpg" << endl;
    cout << "  --titlegraphic=FILE      Use FILE as the .epub cover image" << endl;
    cout << "\nConfig file (.verselumen in current directory, [gospelshare] section):" << endl;
    cout << "  --saveconfig             Save current settings to .verselumen [gospelshare] as new defaults" << endl;
    cout << "  --showconfig             Print current effective settings and exit" << endl;
    cout << "  Supported keys:  bv  tractname  refstyle  pdfmargin  pdffont  pdffontsize  outputtype" << endl;
    cout << "\nExamples:" << endl;
    cout << "  gospelshare --outputtype=md                            Display tract output as markdown" << endl;
    cout << "  gospelshare                                            Display default tract in KJV" << endl;
    cout << "  gospelshare -bv=BSB                                    Display default tract in BSB" << endl;
    cout << "  gospelshare -tn=\"The Romans Road\" -bv=KJV              Display Romans Road in KJV" << endl;
    cout << "  gospelshare --ref=\"John 3:16\"                          Display a single verse" << endl;
    cout << "  gospelshare --ref=\"John 3:16,Romans 8:9-10\"           Display multiple verses" << endl;
    cout << "  gospelshare --ref=\"Romans 8:20-\"                       Display verse 20 to end of chapter" << endl;
    cout << "  gospelshare --ref=\"Romans 8\" -vn                       Display a full chapter with verse numbers" << endl;
    cout << "  gospelshare --output=tract.pdf                         Save tract as PDF (requires pandoc)" << endl;
    cout << "  gospelshare --ref=\"John 3:16\" --output=verse.pdf       Save verse as PDF (requires pandoc)" << endl;
    cout << "  gospelshare --output=tract.epub                        Save tract as EPUB (requires pandoc)" << endl;
    cout << "  gospelshare --output=tract.epub --titlegraphic         Save EPUB with auto-named cover image" << endl;
    cout << "  gospelshare --output=tract.epub --titlegraphic=my.jpg  Save EPUB with specified cover image" << endl;
    cout << "  gospelshare --outputall                                Output .txt/.md/.pdf/.epub for all tracts and versions" << endl;
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

    // ── Load config file first; command-line args override these values ───
    map<string, string> cfg = loadConfig();

#ifdef __APPLE__
    const string defaultPdfFont = "Palatino";
#elif defined(_WIN32)
    const string defaultPdfFont = "Palatino Linotype";
#else
    const string defaultPdfFont = "";
#endif

    string version   = cfgGet(cfg, "bv",          "KJV");
    string tractName = cfgGet(cfg, "tractname",    "The Romans Road");
    string outputType= cfgGet(cfg, "outputtype",   "plaintext");
    string pdfMargin = cfgGet(cfg, "pdfmargin",    "0.5in");
    string pdfFont   = cfgGet(cfg, "pdffont",      defaultPdfFont);
    int pdfFontSizePct = stoi(cfgGet(cfg, "pdffontsize", "100"));
    int refStyle     = stoi(cfgGet(cfg, "refstyle", "1"));

    string refArg;
    string outputFile;
    bool verseNumbers = false;
    bool verseNewline = false;
    bool italic = false;
    bool printPdf = false;
    bool verseQuotes = false;
    bool chapterHeader = false;
    bool saveConfig = false;
    bool showConfig = false;
    bool refStyleExplicit = false;
    bool verseQuotesExplicit = false;
    bool outputAll = false;
    bool useTitleGraphic = false;
    string titleGraphicPath;   // empty = use default slug-based name

    // Parse command-line arguments
    for(int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            cout << "gospelshare v" << VERSION << endl;
            return 0;
        } else if (arg.find("-bv=") == 0) {
            size_t eq = arg.find('=');
            if (eq != string::npos) {
                version = arg.substr(eq + 1);
            }
        } else if (arg.find("--bibleversion=") == 0) {
            size_t eq = arg.find('=');
            if (eq != string::npos) {
                version = arg.substr(eq + 1);
            }
        } else if (arg.find("-tn=") == 0) {
            size_t eq = arg.find('=');
            if (eq != string::npos) {
                tractName = arg.substr(eq + 1);
            }
        } else if (arg.find("--tractname=") == 0) {
            size_t eq = arg.find('=');
            if (eq != string::npos) {
                tractName = arg.substr(eq + 1);
            }
        } else if (arg.find("--outputtype=") == 0) {
            size_t eq = arg.find('=');
            if (eq != string::npos) {
                outputType = arg.substr(eq + 1);
            }
        } else if (arg.find("--ref=") == 0) {
            size_t eq = arg.find('=');
            if (eq != string::npos) {
                refArg = arg.substr(eq + 1);
            }
        } else if (arg.find("--refstyle=") == 0) {
            size_t eq = arg.find('=');
            if (eq != string::npos) {
                refStyle = stoi(arg.substr(eq + 1));
                refStyleExplicit = true;
            }
        } else if (arg == "--versenumbers" || arg == "-vn") {
            verseNumbers = true;
        } else if (arg == "--versenewline" || arg == "-vnl") {
            verseNewline = true;
        } else if (arg.find("--output=") == 0) {
            size_t eq = arg.find('=');
            if (eq != string::npos) {
                outputFile = arg.substr(eq + 1);
            }
        } else if (arg.find("--pdfmargin=") == 0 || arg.find("-pdfmargin=") == 0) {
            size_t eq = arg.find('=');
            if (eq != string::npos) {
                pdfMargin = arg.substr(eq + 1);
            }
        } else if (arg.find("--pdffont=") == 0 || arg.find("-pdffont=") == 0) {
            size_t eq = arg.find('=');
            if (eq != string::npos) {
                pdfFont = arg.substr(eq + 1);
            }
        } else if (arg.find("--pdffontsize=") == 0 || arg.find("-pdffontsize=") == 0) {
            size_t eq = arg.find('=');
            if (eq != string::npos) {
                pdfFontSizePct = stoi(arg.substr(eq + 1));
            }
        } else if (arg == "--italic") {
            italic = true;
        } else if (arg == "--versequotes") {
            verseQuotes = true;
            verseQuotesExplicit = true;
        } else if (arg == "--chapterheader" || arg == "-ch") {
            chapterHeader = true;
        } else if (arg == "--print") {
            printPdf = true;
        } else if (arg == "--tractlist" || arg == "-tl") {
            for (const auto& t : availableTracts) cout << t.first << endl;
            return 0;
        } else if (arg == "--tractpick" || arg == "-tp") {
            string picked = tractPick();
            if (picked.empty()) return 0;
            tractName = picked;
        } else if (arg == "--saveconfig") {
            saveConfig = true;
        } else if (arg == "--showconfig") {
            showConfig = true;
        } else if (arg == "--outputall") {
            outputAll = true;
        } else if (arg == "--titlegraphic") {
            useTitleGraphic = true;
        } else if (arg.find("--titlegraphic=") == 0) {
            useTitleGraphic = true;
            titleGraphicPath = arg.substr(arg.find('=') + 1);
        } else if (arg.find("-") == 0) {
            cerr << "Error: unknown option '" << arg << "'" << endl;
            cerr << "Run 'gospelshare --help' for usage." << endl;
            return 1;
        }
    }

    // Apply per-tract defaults for settings not explicitly set on the CLI
    {
        auto it = tractDefaults.find(tractName);
        if (it == tractDefaults.end()) {
            // try case-insensitive match
            auto toLower = [](string s) { transform(s.begin(), s.end(), s.begin(), ::tolower); return s; };
            string tractNameLower = toLower(tractName);
            for (const auto& td : tractDefaults) {
                if (toLower(td.first) == tractNameLower) { it = tractDefaults.find(td.first); break; }
            }
        }
        if (it != tractDefaults.end()) {
            if (!refStyleExplicit && it->second.refStyle >= 0)
                refStyle = it->second.refStyle;
            if (!verseQuotesExplicit && it->second.verseQuotes >= 0)
                verseQuotes = (it->second.verseQuotes != 0);
        }
    }

    // ── --showconfig: print effective settings and exit ───────────────────
    if (showConfig) {
        cout << "Effective settings (config file + command-line):" << endl;
        cout << "  bv           = " << version       << endl;
        cout << "  tractname    = " << tractName      << endl;
        cout << "  refstyle     = " << refStyle       << endl;
        cout << "  outputtype   = " << outputType     << endl;
        cout << "  pdfmargin    = " << pdfMargin      << endl;
        cout << "  pdffont      = " << pdfFont        << endl;
        cout << "  pdffontsize  = " << pdfFontSizePct << endl;
        ifstream check(CONFIG_FILE);
        if (check.good())
            cout << "\nConfig file: ./" << CONFIG_FILE << " (loaded)" << endl;
        else
            cout << "\nConfig file: ./" << CONFIG_FILE << " (not found — using defaults)" << endl;
        return 0;
    }

    // ── --saveconfig: write [gospelshare] section of .verselumen and exit ─────
    if (saveConfig) {
        vector<string> lines = {
            "bv          = " + version,
            "tractname   = " + tractName,
            "refstyle    = " + to_string(refStyle),
            "outputtype  = " + outputType,
            "pdfmargin   = " + pdfMargin,
            "pdffont     = " + pdfFont,
            "pdffontsize = " + to_string(pdfFontSizePct)
        };
        if (!writeSection(lines)) { cerr << "Error: could not write '" << CONFIG_FILE << "'." << endl; return 1; }
        cerr << "Saved [" << SECTION << "] to ./" << CONFIG_FILE << endl;
        return 0;
    }

    // Normalize version to uppercase for comparison
    transform(version.begin(), version.end(), version.begin(), ::toupper);

    // --outputall: generate .txt, .md, .pdf for every tract × Bible version
    if (outputAll) {
        const vector<pair<string,string>> allVersions = {
            {"KJV", "BibleKJV.txt"},
            {"BSB", "BibleBSB.txt"},
            {"WEB", "BibleWEB.txt"}
        };
        // Collect tract names in a stable order
        vector<string> tractNames;
        for (const auto& t : availableTracts) tractNames.push_back(t.first);

        for (const auto& vp : allVersions) {
            const string& ver = vp.first;
            string bFile      = vp.second;
            ifstream test(bFile);
            if (!test.good()) {
                const char* home = getenv(HOME_ENV);
                if (home) {
                    string homePath = string(home) + "/" + bFile;
                    ifstream homeTest(homePath);
                    if (homeTest.good()) { bFile = homePath; goto version_ready; }
                }
                cerr << "Skipping " << ver << ": '" << bFile << "' not found." << endl;
                continue;
            }
            version_ready:
            bibleVerses = loadBible(bFile);

            for (const auto& tName : tractNames) {
                const Tract& tract = availableTracts.at(tName);

                // Apply per-tract defaults
                int tractRefStyle = refStyle;
                bool tractVerseQuotes = verseQuotes;
                auto dit = tractDefaults.find(tName);
                if (dit != tractDefaults.end()) {
                    if (dit->second.refStyle >= 0)    tractRefStyle   = dit->second.refStyle;
                    if (dit->second.verseQuotes >= 0) tractVerseQuotes = (dit->second.verseQuotes != 0);
                }

                // Build filename base: lowercase tract name, spaces removed, + "_" + version
                string base = tName;
                transform(base.begin(), base.end(), base.begin(), ::tolower);
                base.erase(remove(base.begin(), base.end(), ' '), base.end());
                base += "_" + ver;

                // .txt
                {
                    string content = generateTractContent(tract, ver, false, false, tractRefStyle, false, tractVerseQuotes, false);
                    ofstream f(base + ".txt");
                    if (f) { f << content; cerr << "Saved " << base << ".txt" << endl; }
                    else   { cerr << "Error: could not write '" << base << ".txt'." << endl; }
                }

                // .md
                {
                    string content = generateTractContent(tract, ver, true, false, tractRefStyle, false, tractVerseQuotes, false);
                    ofstream f(base + ".md");
                    if (f) { f << content; cerr << "Saved " << base << ".md" << endl; }
                    else   { cerr << "Error: could not write '" << base << ".md'." << endl; }
                }

                // .pdf
                {
                    pdfColorRegistry.clear();
                    pdfColorCounter = 0;
                    string content = generateTractContent(tract, ver, true, true, tractRefStyle, false, tractVerseQuotes, false);
                    writePdfFile(content, base + ".pdf", pdfMargin, pdfFont, pdfFontSizePct);
                }

                // .epub
                {
                    pdfColorRegistry.clear();
                    pdfColorCounter = 0;
                    string content = generateTractContent(tract, ver, true, false, tractRefStyle, false, tractVerseQuotes, false);
                    string cover = useTitleGraphic
                        ? (titleGraphicPath.empty() ? tractSlug(tract.name) + "_1.jpg" : titleGraphicPath)
                        : "";
                    writeEpubFile(content, base + ".epub", cover, tract.name);
                }
            }
        }
        return 0;
    }

    // Load Bible verses based on selected version
    string bibleFile;
    string bibleUrl;
    if (version == "KJV") {
        bibleFile = "BibleKJV.txt";
        bibleUrl  = "https://openbible.com/textfiles/kjv.txt";
    } else if (version == "BSB") {
        bibleFile = "BibleBSB.txt";
        bibleUrl  = "https://bereanbible.com/bsb.txt";
    } else if (version == "WEB") {
        bibleFile = "BibleWEB.txt";
        bibleUrl  = "https://openbible.com/textfiles/web.txt";
    } else {
        cerr << "Error: unsupported Bible version '" << version << "'." << endl;
        cerr << "Supported versions: KJV, BSB, WEB" << endl;
        return 1;
    }

    // Check that the Bible file exists; offer to download it if not
    {
        ifstream test(bibleFile);
        if (!test.good()) {
            // Try $HOME directory
            const char* home = getenv(HOME_ENV);
            if (home) {
                string homePath = string(home) + "/" + bibleFile;
                ifstream homeTest(homePath);
                if (homeTest.good()) {
                    bibleFile = homePath;
                    goto bible_ready;
                }
            }
            cerr << "Bible file '" << bibleFile << "' not found." << endl;
            cerr << "Download it now? (y/n): ";
            char answer;
            cin >> answer;
            if (answer == 'y' || answer == 'Y') {
                string cmd = "curl -L \"" + bibleUrl + "\" -o \"" + bibleFile + "\"";
                int ret = system(cmd.c_str());
                if (ret != 0) {
                    cerr << "Download failed. Please download manually:" << endl;
                    cerr << "  " << bibleUrl << endl;
                    return 1;
                }
                cout << "Downloaded " << bibleFile << " successfully." << endl;
            } else {
                cerr << "Cannot continue without a Bible file. Exiting." << endl;
                return 1;
            }
        }
    }
    bible_ready:

    bibleVerses = loadBible(bibleFile);

    // PDF output always requires markdown as the intermediate format
    bool isPdf = outputFile.size() >= 4 &&
                 outputFile.substr(outputFile.size() - 4) == ".pdf";
    bool isEpub = outputFile.size() >= 5 &&
                  outputFile.substr(outputFile.size() - 5) == ".epub";
    bool isMd = outputFile.size() >= 3 &&
                outputFile.substr(outputFile.size() - 3) == ".md";
    bool markdown = isPdf || isEpub || isMd || (outputType == "md" || outputType == "MD");

    // Capture all output into a string so we can route it to stdout or a file
    ostringstream out;

    // --ref mode: output only the requested Bible references
    if (!refArg.empty()) {
        stringstream ss(refArg);
        string token;
        string lastBook; // track last book name for shorthand like "Psalm 7, 27"
        while (getline(ss, token, ',')) {
            // Trim optional [] wrappers
            if (!token.empty() && token.front() == '[') token.erase(0, 1);
            if (!token.empty() && token.back()  == ']') token.pop_back();
            // Trim whitespace
            size_t start = token.find_first_not_of(" \t");
            size_t end   = token.find_last_not_of(" \t");
            if (start == string::npos) continue;
            token = token.substr(start, end - start + 1);

            // If token starts with a digit, prepend the last book name
            // e.g. "Psalm 7, 27" → token "27" becomes "Psalm 27"
            if (!token.empty() && isdigit((unsigned char)token[0]) && !lastBook.empty()) {
                token = lastBook + " " + token;
            } else {
                // Extract book name: everything up to the last space before the chapter/verse number
                size_t bookEnd = string::npos;
                for (size_t i = 0; i < token.size(); ++i) {
                    if (isdigit((unsigned char)token[i])) {
                        // walk back past any spaces to find end of book name
                        size_t j = i;
                        while (j > 0 && token[j-1] == ' ') --j;
                        if (j > 0) bookEnd = j;
                        break;
                    }
                }
                if (bookEnd != string::npos)
                    lastBook = token.substr(0, bookEnd);
            }

            string verseText = lookupVerses(token, verseNumbers, verseNewline, markdown);
            if (!verseText.empty()) {
                if (chapterHeader && token.find(':') == string::npos)
                    out << (markdown ? "## " : "") << token << "\n\n";
                out << formatCitation(verseText, token, version, markdown, refStyle, italic, verseQuotes) << endl << endl;
            } else {
                cerr << "Reference not found: " << token << endl;
            }
        }
    } else {
        // Case-insensitive tract name lookup
        auto toLower = [](string s) {
            transform(s.begin(), s.end(), s.begin(), ::tolower);
            return s;
        };
        string tractNameLower = toLower(tractName);
        for (const auto& tract : availableTracts) {
            if (toLower(tract.first) == tractNameLower) {
                tractName = tract.first;
                break;
            }
        }

        // Check if tract exists
        if (availableTracts.find(tractName) == availableTracts.end()) {
            cerr << "Error: Tract '" << tractName << "' not found." << endl;
            cerr << "Available tracts:" << endl;
            for (const auto& tract : availableTracts) {
                cerr << "  - " << tract.first << endl;
            }
            return 1;
        }

        const Tract& selectedTract = availableTracts[tractName];
        out << generateTractContent(selectedTract, version, markdown, isPdf, refStyle, verseNumbers, verseQuotes, verseNewline);
    }

    // Route output
    if (outputFile.empty()) {
        cout << out.str();
    } else if (isPdf) {
        // Write markdown to a temp file then convert via pandoc
        string tmpFile = outputFile + ".tmp.md";
        ofstream tmp(tmpFile, ios::binary);
        if (!tmp) {
            cerr << "Error: could not create temporary file '" << tmpFile << "'." << endl;
            return 1;
        }
        // Write UTF-8 BOM so pandoc correctly identifies the encoding.
        // Without this, MSVC-compiled builds (execution charset = Windows-1252
        // unless /utf-8 is passed) produce non-UTF-8 bytes that pandoc
        // misidentifies, causing missing-character warnings in xelatex.
        tmp.write("\xEF\xBB\xBF", 3);
        tmp << out.str();
        tmp.close();

        // Build pandoc command
        string cmd = "pandoc -f markdown+raw_tex -V geometry:margin=" + pdfMargin;
        string headerFile = outputFile + ".tmp.tex";

        // Use xelatex + fontspec Scale to handle font and font size together.
        // Scale= is relative to the document base (11pt), so 150% = Scale=1.5.
        // This is more reliable than \fontsize with xelatex since fontspec
        // controls font rendering and overrides \fontsize in AtBeginDocument.
        double scale = pdfFontSizePct / 100.0;
        bool needHeader = !pdfFont.empty() || !pdfColorRegistry.empty();
        if (needHeader) {
            if (!pdfFont.empty()) {
                // Use xelatex and set the font + scale together in a header file.
                // We do NOT pass -V mainfont= to pandoc because the template calls
                // \setmainfont before -H header includes, which would ignore Scale.
                // Instead we load fontspec and call \setmainfont[Scale=X]{Font} ourselves.
                cmd += " --pdf-engine=xelatex";
            }
            ofstream hdr(headerFile);
            if (!hdr) {
                cerr << "Error: could not create header file '" << headerFile << "'." << endl;
                remove(tmpFile.c_str());
                return 1;
            }
            if (!pdfFont.empty()) {
                hdr << "\\usepackage{fontspec}\n";
                hdr << "\\setmainfont[Scale=" << scale << "]{" << pdfFont << "}\n";
            }
            if (!pdfColorRegistry.empty()) {
                hdr << "\\usepackage{xcolor}\n";
                for (const auto& entry : pdfColorRegistry) {
                    hdr << "\\definecolor{" << entry.second << "}{HTML}{" << entry.first << "}\n";
                }
            }
            hdr.close();
            cmd += " -H \"" + headerFile + "\"";
        }
        cmd += " \"" + tmpFile + "\" -o \"" + outputFile + "\"";
        int ret = system(cmd.c_str());
        remove(tmpFile.c_str());
        remove(headerFile.c_str());

        if (ret != 0) {
            cerr << "Error: PDF conversion failed." << endl;
            cerr << endl;
            cerr << "PDF generation requires pandoc and a LaTeX engine." << endl;
            cerr << "Install both with:" << endl;
            cerr << "  macOS:   brew install pandoc && brew install --cask basictex" << endl;
            cerr << "  Linux:   apt install pandoc texlive" << endl;
            cerr << "  Windows: winget install pandoc && winget install MiKTeX.MiKTeX" << endl;
            cerr << endl;
            cerr << "After installing basictex (macOS), run: sudo tlmgr update --self" << endl;
            cerr << "Then open a new Terminal window — pdflatex is not found in existing sessions." << endl;
            cerr << "Or generate markdown and convert manually:" << endl;
            cerr << "  gospelshare --outputtype=md | pandoc -f markdown -o " << outputFile << endl;
            return 1;
        }
        cerr << "Saved to " << outputFile << endl;
        if (printPdf) {
#ifdef _WIN32
            string printCmd = "powershell -Command \"Start-Process -FilePath '" +
                              outputFile + "' -Verb Print -Wait\"";
            int printRet = system(printCmd.c_str());
            if (printRet != 0) {
                cerr << "Warning: print command failed." << endl;
                cerr << "Make sure a PDF viewer that supports the Print verb is installed." << endl;
            } else {
                cerr << "Sent to printer." << endl;
            }
#else
            // Find the first available printer to avoid stale ~/.cups/lpoptions issues
            FILE* pipe = popen("lpstat -p 2>/dev/null | awk '/^printer/ {print $2; exit}'", "r");
            string printer;
            if (pipe) {
                char buf[256];
                if (fgets(buf, sizeof(buf), pipe)) {
                    printer = buf;
                    // trim trailing newline
                    if (!printer.empty() && printer.back() == '\n')
                        printer.pop_back();
                }
                pclose(pipe);
            }
            string printCmd;
            if (!printer.empty()) {
                printCmd = "lpr -P \"" + printer + "\" \"" + outputFile + "\"";
            } else {
                printCmd = "lpr \"" + outputFile + "\"";
            }
            int printRet = system(printCmd.c_str());
            if (printRet != 0) {
                cerr << "Warning: print command failed." << endl;
                cerr << "If you see a lpoptions error, fix it with: rm ~/.cups/lpoptions" << endl;
                cerr << "To list available printers: lpstat -p" << endl;
            } else {
                if (!printer.empty())
                    cerr << "Sent to printer: " << printer << endl;
                else
                    cerr << "Sent to printer." << endl;
            }
#endif
        }
    } else if (isEpub) {
        string cover = useTitleGraphic
            ? (titleGraphicPath.empty() ? tractSlug(tractName) + "_1.jpg" : titleGraphicPath)
            : "";
        if (!writeEpubFile(out.str(), outputFile, cover, tractName)) {
            cerr << endl;
            cerr << "EPUB generation requires pandoc." << endl;
            cerr << "Install with:" << endl;
            cerr << "  macOS:   brew install pandoc" << endl;
            cerr << "  Linux:   apt install pandoc" << endl;
            cerr << "  Windows: winget install pandoc" << endl;
            return 1;
        }
    } else {
        ofstream f(outputFile);
        if (!f) {
            cerr << "Error: could not write to '" << outputFile << "'." << endl;
            return 1;
        }
        f << out.str();
        cerr << "Saved to " << outputFile << endl;
    }

    return 0;
}