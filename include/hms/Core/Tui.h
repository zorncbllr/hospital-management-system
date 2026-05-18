#ifndef HMS_CORE_TUI_H
#define HMS_CORE_TUI_H

#include <string>
#include <vector>

namespace hms::tui {

enum class Level { Info, Success, Warning, Error };

namespace color {
    constexpr const char* RESET   = "\033[0m";
    constexpr const char* BOLD    = "\033[1m";
    constexpr const char* DIM     = "\033[2m";

    constexpr const char* BLACK   = "\033[30m";
    constexpr const char* RED     = "\033[31m";
    constexpr const char* GREEN   = "\033[32m";
    constexpr const char* YELLOW  = "\033[33m";
    constexpr const char* BLUE    = "\033[34m";
    constexpr const char* MAGENTA = "\033[35m";
    constexpr const char* CYAN    = "\033[36m";
    constexpr const char* WHITE   = "\033[37m";

    constexpr const char* BG_BLUE  = "\033[44m";
    constexpr const char* BG_CYAN  = "\033[46m";
}

void clearScreen();
void pause(const std::string& msg = "Press Enter to continue...");

void banner(const std::string& title,
            const std::string& subtitle = "",
            const std::vector<std::string>& breadcrumbPath = {});
void sectionHeader(const std::string& title);
void hintBar(const std::vector<std::string>& hints);

void toast(const std::string& message, Level level = Level::Info);
bool confirm(const std::string& message);

struct MenuOption {
    char key;
    std::string label;
    std::string hint;
};

char menu(const std::string& title,
          const std::vector<std::string>& breadcrumbPath,
          const std::vector<MenuOption>& options,
          const std::string& footnote = "");

int tableBoxWidth(const std::vector<std::string>& headers,
                  const std::vector<std::vector<std::string>>& rows,
                  const std::string& tableTitle = "");

int bannerOpen(const std::string& title,
               const std::string& subtitle = "",
               const std::vector<std::string>& breadcrumbPath = {},
               int minWidth = 0);

void tableInBox(int boxWidth,
                const std::vector<std::string>& headers,
                const std::vector<std::vector<std::string>>& rows,
                const std::string& sectionTitle = "",
                bool closeBox = true);

void table(const std::vector<std::string>& headers,
           const std::vector<std::vector<std::string>>& rows,
           const std::string& title = "");

void bar(double ratio, int width = 20);

std::string repeat(const std::string& s, int n);
std::string padRight(const std::string& s, std::size_t width);
std::string padLeft(const std::string& s, std::size_t width);
std::string center(const std::string& s, std::size_t width);

}

#endif
