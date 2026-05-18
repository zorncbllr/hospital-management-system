#include <hms/Core/Tui.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace hms::tui {

static std::size_t visibleLen(const std::string& s) {
    std::size_t len = 0;
    bool inEsc = false;
    for (unsigned char c : s) {
        if (c == '\033') { inEsc = true; continue; }
        if (inEsc) {
            if (c == 'm') inEsc = false;
            continue;
        }
        // Skip UTF-8 continuation bytes (10xxxxxx) so multi-byte chars count as 1
        if ((c & 0xC0) != 0x80) ++len;
    }
    return len;
}

std::string repeat(const std::string& s, int n) {
    std::string out;
    out.reserve(s.size() * static_cast<std::size_t>(std::max(0, n)));
    for (int i = 0; i < n; ++i) out += s;
    return out;
}

std::string padRight(const std::string& s, std::size_t width) {
    std::size_t len = visibleLen(s);
    if (len >= width) return s;
    return s + std::string(width - len, ' ');
}

std::string padLeft(const std::string& s, std::size_t width) {
    std::size_t len = visibleLen(s);
    if (len >= width) return s;
    return std::string(width - len, ' ') + s;
}

std::string center(const std::string& s, std::size_t width) {
    std::size_t len = visibleLen(s);
    if (len >= width) return s;
    std::size_t total = width - len;
    std::size_t left = total / 2;
    std::size_t right = total - left;
    return std::string(left, ' ') + s + std::string(right, ' ');
}

void clearScreen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

void pause(const std::string& msg) {
    std::cout << color::DIM << msg << color::RESET << std::flush;
    std::string dummy;
    std::getline(std::cin, dummy);
}

static const int BOX_WIDTH = 64;

static void hLine(char left, char fill, char right, int width) {
    std::cout << left;
    for (int i = 0; i < width - 2; ++i) std::cout << fill;
    std::cout << right << "\n";
}

static void boxTop(int width = BOX_WIDTH) {
    std::cout << color::WHITE << "╔";
    for (int i = 0; i < width - 2; ++i) std::cout << "═";
    std::cout << "╗" << color::RESET << "\n";
    (void)hLine;
}

static void boxMid(int width = BOX_WIDTH) {
    std::cout << color::WHITE << "╠";
    for (int i = 0; i < width - 2; ++i) std::cout << "═";
    std::cout << "╣" << color::RESET << "\n";
}

static void boxBottom(int width = BOX_WIDTH) {
    std::cout << color::WHITE << "╚";
    for (int i = 0; i < width - 2; ++i) std::cout << "═";
    std::cout << "╝" << color::RESET << "\n";
}

static void boxRow(const std::string& content, int width = BOX_WIDTH) {
    int inner = width - 2;
    std::cout << color::WHITE << "║" << color::RESET
              << padRight(content, static_cast<std::size_t>(inner))
              << color::WHITE << "║" << color::RESET << "\n";
}

static void boxRowCentered(const std::string& content, int width = BOX_WIDTH) {
    int inner = width - 2;
    std::cout << color::WHITE << "║" << color::RESET
              << center(content, static_cast<std::size_t>(inner))
              << color::WHITE << "║" << color::RESET << "\n";
}

void banner(const std::string& title, const std::string& subtitle,
            const std::vector<std::string>& breadcrumbPath) {
    static const std::string APP_TITLE = "HOSPITAL MANAGEMENT SYSTEM";
    int w = BOX_WIDTH;
    w = std::max(w, static_cast<int>(visibleLen(APP_TITLE) + 4));
    w = std::max(w, static_cast<int>(visibleLen(title) + 4));

    boxTop(w);
    boxRowCentered(std::string(color::BOLD) + APP_TITLE + color::RESET, w);
    if (!breadcrumbPath.empty()) {
        boxMid(w);
        std::string crumb = "  ";
        for (std::size_t i = 0; i < breadcrumbPath.size(); ++i) {
            if (i > 0) crumb += " › ";
            crumb += breadcrumbPath[i];
        }
        boxRow(std::string(color::DIM) + crumb + color::RESET, w);
    }
    boxMid(w);
    boxRowCentered(std::string(color::BOLD) + title + color::RESET, w);
    if (!subtitle.empty()) {
        boxRowCentered(std::string(color::DIM) + subtitle + color::RESET, w);
    }
    boxBottom(w);
}

int tableBoxWidth(const std::vector<std::string>& headers,
                  const std::vector<std::vector<std::string>>& rows,
                  const std::string& tableTitle) {
    if (headers.empty()) return BOX_WIDTH;
    const int N = static_cast<int>(headers.size());
    std::vector<std::size_t> widths(static_cast<std::size_t>(N), 0);
    for (int i = 0; i < N; ++i)
        widths[static_cast<std::size_t>(i)] = visibleLen(headers[static_cast<std::size_t>(i)]);
    for (const auto& row : rows)
        for (int i = 0; i < N && i < static_cast<int>(row.size()); ++i)
            widths[static_cast<std::size_t>(i)] = std::max(
                widths[static_cast<std::size_t>(i)],
                visibleLen(row[static_cast<std::size_t>(i)]));
    std::size_t inner = static_cast<std::size_t>(N - 1);
    for (auto w : widths) inner += w + 2;
    if (!tableTitle.empty())
        inner = std::max(inner, visibleLen(tableTitle) + 2);
    return static_cast<int>(inner) + 2;
}

int bannerOpen(const std::string& title, const std::string& subtitle,
               const std::vector<std::string>& breadcrumbPath, int minWidth) {
    static const std::string APP_TITLE = "HOSPITAL MANAGEMENT SYSTEM";
    int w = BOX_WIDTH;
    w = std::max(w, static_cast<int>(visibleLen(APP_TITLE)) + 4);
    w = std::max(w, static_cast<int>(visibleLen(title)) + 4);
    if (minWidth > 0) w = std::max(w, minWidth);

    boxTop(w);
    boxRowCentered(std::string(color::BOLD) + APP_TITLE + color::RESET, w);
    if (!breadcrumbPath.empty()) {
        boxMid(w);
        std::string crumb = "  ";
        for (std::size_t i = 0; i < breadcrumbPath.size(); ++i) {
            if (i > 0) crumb += " › ";
            crumb += breadcrumbPath[i];
        }
        boxRow(std::string(color::DIM) + crumb + color::RESET, w);
    }
    boxMid(w);
    boxRowCentered(std::string(color::BOLD) + title + color::RESET, w);
    if (!subtitle.empty())
        boxRowCentered(std::string(color::DIM) + subtitle + color::RESET, w);
    return w;
}

void tableInBox(int boxWidth,
                const std::vector<std::string>& headers,
                const std::vector<std::vector<std::string>>& rows,
                const std::string& sectionTitle,
                bool closeBox) {
    if (headers.empty()) { boxBottom(boxWidth); return; }
    const int N = static_cast<int>(headers.size());
    const std::size_t innerWidth = static_cast<std::size_t>(boxWidth - 2);

    std::vector<std::size_t> widths(static_cast<std::size_t>(N), 0);
    for (int i = 0; i < N; ++i)
        widths[static_cast<std::size_t>(i)] = visibleLen(headers[static_cast<std::size_t>(i)]);
    for (const auto& row : rows)
        for (int i = 0; i < N && i < static_cast<int>(row.size()); ++i)
            widths[static_cast<std::size_t>(i)] = std::max(
                widths[static_cast<std::size_t>(i)],
                visibleLen(row[static_cast<std::size_t>(i)]));

    std::size_t natInner = static_cast<std::size_t>(N - 1);
    for (auto w : widths) natInner += w + 2;
    if (innerWidth > natInner)
        widths[static_cast<std::size_t>(N - 1)] += innerWidth - natInner;

    auto colSep = [&](const char* left, const char* mid, const char* right) {
        std::cout << color::WHITE << left;
        for (int i = 0; i < N; ++i) {
            for (std::size_t j = 0; j < widths[static_cast<std::size_t>(i)] + 2; ++j)
                std::cout << "═";
            std::cout << (i < N - 1 ? mid : right);
        }
        std::cout << color::RESET << "\n";
    };
    auto headerRow = [&]() {
        std::cout << color::WHITE << "║" << color::RESET;
        for (int i = 0; i < N; ++i) {
            std::cout << " " << color::BOLD
                      << padRight(headers[static_cast<std::size_t>(i)],
                                  widths[static_cast<std::size_t>(i)])
                      << color::RESET << " ";
            std::cout << color::WHITE << (i < N - 1 ? "│" : "║") << color::RESET;
        }
        std::cout << "\n";
    };
    auto dataRow = [&](const std::vector<std::string>& row) {
        std::cout << color::WHITE << "║" << color::RESET;
        for (int i = 0; i < N; ++i) {
            std::string cell = (i < static_cast<int>(row.size()))
                ? row[static_cast<std::size_t>(i)] : std::string("");
            std::cout << " " << padRight(cell, widths[static_cast<std::size_t>(i)]) << " ";
            std::cout << color::WHITE << (i < N - 1 ? "│" : "║") << color::RESET;
        }
        std::cout << "\n";
    };

    if (!sectionTitle.empty()) {
        std::cout << color::WHITE << "║" << color::RESET
                  << padRight(" " + std::string(color::BOLD) + sectionTitle + color::RESET,
                              innerWidth)
                  << color::WHITE << "║" << color::RESET << "\n";
    }
    colSep("╠", "╤", "╣");
    headerRow();
    colSep("╠", "╪", "╣");
    if (rows.empty()) {
        std::cout << color::WHITE << "║" << color::RESET
                  << center(" (no records) ", innerWidth)
                  << color::WHITE << "║" << color::RESET << "\n";
    } else {
        for (const auto& row : rows) dataRow(row);
    }
    if (closeBox) colSep("╚", "╧", "╝");
    else          colSep("╠", "╧", "╣");
}

void sectionHeader(const std::string& title) {
    std::cout << "\n" << color::BOLD << color::WHITE
              << "▶ " << title << color::RESET << "\n"
              << color::DIM
              << repeat("─", static_cast<int>(title.size()) + 2)
              << color::RESET << "\n";
}

void hintBar(const std::vector<std::string>& hints) {
    std::cout << color::DIM << "  ";
    for (std::size_t i = 0; i < hints.size(); ++i) {
        if (i > 0) std::cout << "   ";
        std::cout << hints[i];
    }
    std::cout << color::RESET << "\n";
}

void toast(const std::string& message, Level level) {
    const char* prefix = " ";
    const char* col = color::WHITE;
    switch (level) {
        case Level::Info:    prefix = "[i]"; col = color::WHITE; break;
        case Level::Success: prefix = "[✓]"; col = color::WHITE; break;
        case Level::Warning: prefix = "[!]"; col = color::WHITE; break;
        case Level::Error:   prefix = "[x]"; col = color::WHITE; break;
    }
    std::cout << "\n  " << col << prefix << " " << message
              << color::RESET << "\n\n";
}

bool confirm(const std::string& message) {
    while (true) {
        std::cout << "\n  " << color::WHITE << "[?] " << message
                  << " (y/n): " << color::RESET << std::flush;
        std::string ans;
        std::getline(std::cin, ans);
        if (ans.empty()) {
            toast("Please enter y or n.", Level::Warning);
            continue;
        }
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ans[0])));
        if (c == 'y') return true;
        if (c == 'n') return false;
        toast("Please enter y or n.", Level::Warning);
    }
}

char menu(const std::string& title,
          const std::vector<std::string>& breadcrumbPath,
          const std::vector<MenuOption>& options,
          const std::string& footnote) {
    std::size_t labelColumn = 0;
    for (const auto& opt : options) {
        labelColumn = std::max(labelColumn,
            std::size_t(7) + visibleLen(opt.label));
    }
    labelColumn = std::max<std::size_t>(labelColumn, 28);

    std::size_t hintWidth = 0;
    for (const auto& opt : options) {
        hintWidth = std::max(hintWidth, visibleLen(opt.hint));
    }

    static const std::string APP_TITLE = "HOSPITAL MANAGEMENT SYSTEM";

    int boxWidth = static_cast<int>(labelColumn + 2 + hintWidth + 4);
    boxWidth = std::max(boxWidth, static_cast<int>(visibleLen(title) + 6));
    boxWidth = std::max(boxWidth, static_cast<int>(visibleLen(APP_TITLE) + 4));
    boxWidth = std::max(boxWidth, 50);

    while (true) {
        clearScreen();
        boxTop(boxWidth);
        boxRowCentered(std::string(color::BOLD) + APP_TITLE + color::RESET, boxWidth);
        if (!breadcrumbPath.empty()) {
            boxMid(boxWidth);
            std::string crumb = "  ";
            for (std::size_t i = 0; i < breadcrumbPath.size(); ++i) {
                if (i > 0) crumb += " › ";
                crumb += breadcrumbPath[i];
            }
            boxRow(std::string(color::DIM) + crumb + color::RESET, boxWidth);
        }
        boxMid(boxWidth);
        boxRowCentered(std::string(color::BOLD) + title + color::RESET, boxWidth);
        boxMid(boxWidth);
        for (const auto& opt : options) {
            std::string keyTag = std::string("  [") + opt.key + "]  ";
            std::string line = keyTag + opt.label;
            if (!opt.hint.empty()) {
                std::size_t cur = visibleLen(line);
                std::size_t target = labelColumn + 2;
                std::size_t padLen = (target > cur) ? (target - cur) : 2;
                line += std::string(padLen, ' ') + color::DIM + opt.hint + color::RESET;
            }
            boxRow(line, boxWidth);
        }
        boxMid(boxWidth);
        std::string keysHint = "  ";
        for (std::size_t i = 0; i < options.size(); ++i) {
            if (i > 0) keysHint += " / ";
            keysHint += std::string("[") + options[i].key + "]";
        }
        boxRow(std::string(color::DIM) + keysHint + color::RESET, boxWidth);
        boxBottom(boxWidth);
        if (!footnote.empty()) {
            std::cout << color::DIM << "  " << footnote
                      << color::RESET << "\n";
        }
        std::cout << "\n  " << color::BOLD << "Choose > " << color::RESET
                  << std::flush;

        std::string input;
        std::getline(std::cin, input);
        if (input.empty()) continue;
        char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(input[0])));
        for (const auto& opt : options) {
            char k = static_cast<char>(std::toupper(static_cast<unsigned char>(opt.key)));
            if (k == ch) return opt.key;
        }
        toast(std::string("Unknown choice: '") + input + "'. Try again.",
              Level::Warning);
        pause();
    }
}

void table(const std::vector<std::string>& headers,
           const std::vector<std::vector<std::string>>& rows,
           const std::string& title) {
    if (headers.empty()) return;
    const int N = static_cast<int>(headers.size());

    std::vector<std::size_t> widths(static_cast<std::size_t>(N), 0);
    for (int i = 0; i < N; ++i)
        widths[static_cast<std::size_t>(i)] = visibleLen(headers[static_cast<std::size_t>(i)]);
    for (const auto& row : rows)
        for (int i = 0; i < N && i < static_cast<int>(row.size()); ++i)
            widths[static_cast<std::size_t>(i)] = std::max(
                widths[static_cast<std::size_t>(i)],
                visibleLen(row[static_cast<std::size_t>(i)]));

    // innerWidth = total chars between the left and right ║
    std::size_t innerWidth = static_cast<std::size_t>(N - 1); // col-sep chars
    for (auto w : widths) innerWidth += w + 2;

    // If title provided, ensure innerWidth is wide enough for it
    if (!title.empty()) {
        std::size_t needed = visibleLen(title) + 2;
        if (needed > innerWidth) {
            std::size_t usedByOthers = static_cast<std::size_t>(N - 1);
            for (int i = 0; i < N - 1; ++i)
                usedByOthers += widths[static_cast<std::size_t>(i)] + 2;
            if (needed > usedByOthers + 2)
                widths[static_cast<std::size_t>(N - 1)] = needed - usedByOthers - 2;
            innerWidth = static_cast<std::size_t>(N - 1);
            for (auto w : widths) innerWidth += w + 2;
        }
    }

    auto topWithSep = [&]() {
        std::cout << color::WHITE << "╔";
        for (int i = 0; i < N; ++i) {
            for (std::size_t j = 0; j < widths[static_cast<std::size_t>(i)] + 2; ++j)
                std::cout << "═";
            std::cout << (i < N - 1 ? "╤" : "╗");
        }
        std::cout << color::RESET << "\n";
    };
    auto topNoSep = [&]() {
        std::cout << color::WHITE << "╔";
        for (std::size_t j = 0; j < innerWidth; ++j) std::cout << "═";
        std::cout << "╗" << color::RESET << "\n";
    };
    auto midWithSep = [&]() {
        std::cout << color::WHITE << "╠";
        for (int i = 0; i < N; ++i) {
            for (std::size_t j = 0; j < widths[static_cast<std::size_t>(i)] + 2; ++j)
                std::cout << "═";
            std::cout << (i < N - 1 ? "╪" : "╣");
        }
        std::cout << color::RESET << "\n";
    };
    auto midIntroSep = [&]() {
        std::cout << color::WHITE << "╠";
        for (int i = 0; i < N; ++i) {
            for (std::size_t j = 0; j < widths[static_cast<std::size_t>(i)] + 2; ++j)
                std::cout << "═";
            std::cout << (i < N - 1 ? "╤" : "╣");
        }
        std::cout << color::RESET << "\n";
    };
    auto botLine = [&]() {
        std::cout << color::WHITE << "╚";
        for (int i = 0; i < N; ++i) {
            for (std::size_t j = 0; j < widths[static_cast<std::size_t>(i)] + 2; ++j)
                std::cout << "═";
            std::cout << (i < N - 1 ? "╧" : "╝");
        }
        std::cout << color::RESET << "\n";
    };
    auto headerRow = [&]() {
        std::cout << color::WHITE << "║" << color::RESET;
        for (int i = 0; i < N; ++i) {
            std::cout << " " << color::BOLD
                      << padRight(headers[static_cast<std::size_t>(i)],
                                  widths[static_cast<std::size_t>(i)])
                      << color::RESET << " ";
            std::cout << color::WHITE << (i < N - 1 ? "│" : "║") << color::RESET;
        }
        std::cout << "\n";
    };
    auto dataRow = [&](const std::vector<std::string>& row) {
        std::cout << color::WHITE << "║" << color::RESET;
        for (int i = 0; i < N; ++i) {
            std::string cell = (i < static_cast<int>(row.size()))
                ? row[static_cast<std::size_t>(i)] : std::string("");
            std::cout << " " << padRight(cell, widths[static_cast<std::size_t>(i)]) << " ";
            std::cout << color::WHITE << (i < N - 1 ? "│" : "║") << color::RESET;
        }
        std::cout << "\n";
    };
    auto titleRow = [&](const std::string& t) {
        std::cout << color::WHITE << "║" << color::RESET
                  << padRight(" " + std::string(color::BOLD) + t + color::RESET, innerWidth)
                  << color::WHITE << "║" << color::RESET << "\n";
    };

    std::cout << "\n";
    if (!title.empty()) {
        topNoSep();
        titleRow(title);
        midIntroSep();
    } else {
        topWithSep();
    }
    headerRow();
    midWithSep();
    if (rows.empty()) {
        std::cout << color::WHITE << "║" << color::RESET
                  << center(" (no records) ", innerWidth)
                  << color::WHITE << "║" << color::RESET << "\n";
    } else {
        for (const auto& row : rows) dataRow(row);
    }
    botLine();
}

void bar(double ratio, int width) {
    if (ratio < 0) ratio = 0;
    if (ratio > 1) ratio = 1;
    int filled = static_cast<int>(ratio * width + 0.5);
    const char* col = color::WHITE;
    if (ratio >= 0.85) col = color::WHITE;
    else if (ratio >= 0.6) col = color::WHITE;
    std::cout << col;
    for (int i = 0; i < filled; ++i) std::cout << "█";
    std::cout << color::DIM;
    for (int i = filled; i < width; ++i) std::cout << "░";
    std::cout << color::RESET;
}

}
