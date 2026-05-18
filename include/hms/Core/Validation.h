#ifndef HMS_CORE_VALIDATION_H
#define HMS_CORE_VALIDATION_H

#include <ctime>
#include <string>

namespace hms {

class Tui;

class Validator {
    Tui& tui_;
public:
    explicit Validator(Tui& tui) : tui_(tui) {}

    int         readInt(const std::string& prompt, int min, int max);
    double      readDouble(const std::string& prompt, double min, double max);
    std::string readLine(const std::string& prompt, bool allowEmpty = false);
    std::string readNonEmpty(const std::string& prompt);
    std::string readName(const std::string& prompt);
    std::string readContact(const std::string& prompt);
    std::string readAddress(const std::string& prompt);
    std::string readRoom(const std::string& prompt);
    std::string readSpecialty(const std::string& prompt);
    char        readChar(const std::string& prompt, const std::string& allowed);
    std::time_t readDate(const std::string& prompt);
    int         readTimeOfDay(const std::string& prompt);
    int         readDuration(const std::string& prompt, int minMinutes, int maxMinutes);

    static std::string trim(const std::string& s);
    static bool        isBlank(const std::string& s);

    static std::string formatDate(std::time_t when);
    static std::string formatDateTime(std::time_t when);
    static std::string formatTime(int secondsOfDay);
    static std::time_t parseDate(const std::string& text);
    static int         parseTime(const std::string& text);

    static std::time_t today();
    static std::time_t now();

private:
    std::string prompt(const std::string& label);
    static bool isCancel(const std::string& s);
};

}

#endif
