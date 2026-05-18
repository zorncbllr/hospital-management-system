#ifndef HMS_CORE_TUI_H
#define HMS_CORE_TUI_H

#include <string>
#include <vector>

namespace hms {

class Tui {
public:
    enum class Level { Info, Success, Warning, Error };

    struct Color {
        Color() = delete;
        static constexpr const char* RESET   = "\033[0m";
        static constexpr const char* BOLD    = "\033[1m";
        static constexpr const char* DIM     = "\033[2m";

        static constexpr const char* BLACK   = "\033[30m";
        static constexpr const char* RED     = "\033[31m";
        static constexpr const char* GREEN   = "\033[32m";
        static constexpr const char* YELLOW  = "\033[33m";
        static constexpr const char* BLUE    = "\033[34m";
        static constexpr const char* MAGENTA = "\033[35m";
        static constexpr const char* CYAN    = "\033[36m";
        static constexpr const char* WHITE   = "\033[37m";

        static constexpr const char* BG_BLUE = "\033[44m";
        static constexpr const char* BG_CYAN = "\033[46m";
    };

    struct MenuOption {
        char key;
        std::string label;
        std::string hint;
    };

    Tui() = default;

    void clearScreen();
    void pause(const std::string& msg = "Press Enter to continue...");

    void banner(const std::string& title,
                const std::string& subtitle = "",
                const std::vector<std::string>& breadcrumbPath = {});
    void sectionHeader(const std::string& title);
    void hintBar(const std::vector<std::string>& hints);

    void toast(const std::string& message, Level level = Level::Info);
    bool confirm(const std::string& message);

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

    static std::string repeat(const std::string& s, int n);
    static std::string padRight(const std::string& s, std::size_t width);
    static std::string padLeft(const std::string& s, std::size_t width);
    static std::string center(const std::string& s, std::size_t width);

private:
    static constexpr int BOX_WIDTH = 64;

    static std::size_t visibleLen(const std::string& s);

    void boxTop(int width = BOX_WIDTH);
    void boxMid(int width = BOX_WIDTH);
    void boxBottom(int width = BOX_WIDTH);
    void boxRow(const std::string& content, int width = BOX_WIDTH);
    void boxRowCentered(const std::string& content, int width = BOX_WIDTH);
};

}

#endif
