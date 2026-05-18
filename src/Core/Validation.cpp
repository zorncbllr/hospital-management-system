#include <hms/Core/Exceptions.h>
#include <hms/Core/Tui.h>
#include <hms/Core/Validation.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace hms {

std::string Validator::trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

bool Validator::isBlank(const std::string& s) {
    for (char c : s) if (!std::isspace(static_cast<unsigned char>(c))) return false;
    return true;
}

bool Validator::isCancel(const std::string& s) {
    std::string t = trim(s);
    return t == "c" || t == "C";
}

std::string Validator::prompt(const std::string& label) {
    std::cout << "  " << Tui::Color::BOLD << label << Tui::Color::RESET
              << " " << Tui::Color::DIM << "(c to cancel)" << Tui::Color::RESET
              << " > " << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) {
        throw CancelledException();
    }
    if (isCancel(line)) throw CancelledException();
    return trim(line);
}

int Validator::readInt(const std::string& label, int min, int max) {
    while (true) {
        std::string s = prompt(label);
        if (s.empty()) {
            tui_.toast("Value required.", Tui::Level::Warning);
            continue;
        }
        try {
            std::size_t pos = 0;
            int v = std::stoi(s, &pos);
            if (pos != s.size()) throw std::invalid_argument("trailing");
            if (v < min || v > max) {
                tui_.toast("Enter a number between " + std::to_string(min) +
                           " and " + std::to_string(max) + ".",
                           Tui::Level::Warning);
                continue;
            }
            return v;
        } catch (const std::exception&) {
            tui_.toast("Not a valid number.", Tui::Level::Warning);
        }
    }
}

double Validator::readDouble(const std::string& label, double min, double max) {
    while (true) {
        std::string s = prompt(label);
        if (s.empty()) {
            tui_.toast("Value required.", Tui::Level::Warning);
            continue;
        }
        try {
            std::size_t pos = 0;
            double v = std::stod(s, &pos);
            if (pos != s.size()) throw std::invalid_argument("trailing");
            if (v < min || v > max) {
                std::ostringstream oss;
                oss << "Enter a number between " << min << " and " << max << ".";
                tui_.toast(oss.str(), Tui::Level::Warning);
                continue;
            }
            return v;
        } catch (const std::exception&) {
            tui_.toast("Not a valid number.", Tui::Level::Warning);
        }
    }
}

std::string Validator::readLine(const std::string& label, bool allowEmpty) {
    while (true) {
        std::string s = prompt(label);
        if (s.empty() && !allowEmpty) {
            tui_.toast("Value required.", Tui::Level::Warning);
            continue;
        }
        return s;
    }
}

std::string Validator::readNonEmpty(const std::string& label) {
    return readLine(label, false);
}

std::string Validator::readName(const std::string& label) {
    while (true) {
        std::string s = prompt(label);
        if (s.empty()) {
            tui_.toast("Name is required.", Tui::Level::Warning);
            continue;
        }
        if (s.size() < 2) {
            tui_.toast("Name must be at least 2 characters.", Tui::Level::Warning);
            continue;
        }
        bool valid = true;
        for (char c : s) {
            unsigned char uc = static_cast<unsigned char>(c);
            if (!std::isalpha(uc) && !std::isspace(uc) && c != '-' && c != '\'' && c != '.') {
                valid = false;
                break;
            }
        }
        if (!valid) {
            tui_.toast("Name must contain only letters, spaces, hyphens, apostrophes, and periods.", Tui::Level::Warning);
            continue;
        }
        return s;
    }
}

std::string Validator::readContact(const std::string& label) {
    while (true) {
        std::string s = prompt(label + " (09XXXXXXXXX)");
        if (s.empty()) {
            tui_.toast("Contact number required.", Tui::Level::Warning);
            continue;
        }
        std::string digits;
        for (char c : s) {
            if (std::isdigit(static_cast<unsigned char>(c))) {
                digits += c;
            }
        }
        if (digits.size() != 11) {
            tui_.toast("Contact number must be exactly 11 digits.", Tui::Level::Warning);
            continue;
        }
        if (digits[0] != '0' || digits[1] != '9') {
            tui_.toast("Contact number must start with 09.", Tui::Level::Warning);
            continue;
        }
        return digits;
    }
}

std::string Validator::readAddress(const std::string& label) {
    while (true) {
        std::string s = prompt(label);
        if (s.empty()) {
            tui_.toast("Address required.", Tui::Level::Warning);
            continue;
        }
        if (s.size() < 10) {
            tui_.toast("Address is too short (minimum 10 characters).", Tui::Level::Warning);
            continue;
        }
        if (s.size() > 200) {
            tui_.toast("Address is too long (maximum 200 characters).", Tui::Level::Warning);
            continue;
        }
        return s;
    }
}

std::string Validator::readRoom(const std::string& label) {
    while (true) {
        std::string s = prompt(label);
        if (s.empty()) {
            tui_.toast("Consulting room required.", Tui::Level::Warning);
            continue;
        }
        if (s.size() < 3) {
            tui_.toast("Consulting room must be at least 3 characters.", Tui::Level::Warning);
            continue;
        }
        return s;
    }
}

std::string Validator::readSpecialty(const std::string& label) {
    while (true) {
        std::string s = prompt(label);
        if (s.empty()) {
            tui_.toast("Specialty required.", Tui::Level::Warning);
            continue;
        }
        if (s.size() < 3) {
            tui_.toast("Specialty must be at least 3 characters.", Tui::Level::Warning);
            continue;
        }
        return s;
    }
}

char Validator::readChar(const std::string& label, const std::string& allowed) {
    while (true) {
        std::string s = prompt(label);
        if (s.size() != 1) {
            tui_.toast("Enter one of: " + allowed, Tui::Level::Warning);
            continue;
        }
        char c = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
        for (char a : allowed) {
            char au = static_cast<char>(std::toupper(static_cast<unsigned char>(a)));
            if (c == au) return c;
        }
        tui_.toast("Enter one of: " + allowed, Tui::Level::Warning);
    }
}

std::time_t Validator::parseDate(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-')
        throw InvalidInputException("date must be YYYY-MM-DD");
    int y, m, d;
    try {
        y = std::stoi(s.substr(0, 4));
        m = std::stoi(s.substr(5, 2));
        d = std::stoi(s.substr(8, 2));
    } catch (...) {
        throw InvalidInputException("date must be YYYY-MM-DD");
    }
    if (y < 1900 || y > 2100) throw InvalidInputException("year out of range");
    if (m < 1 || m > 12)      throw InvalidInputException("month out of range");
    if (d < 1 || d > 31)      throw InvalidInputException("day out of range");
    std::tm tm{};
    tm.tm_year = y - 1900;
    tm.tm_mon  = m - 1;
    tm.tm_mday = d;
    tm.tm_hour = 0;
    tm.tm_min  = 0;
    tm.tm_sec  = 0;
    tm.tm_isdst = -1;
    std::time_t t = std::mktime(&tm);
    if (t == static_cast<std::time_t>(-1))
        throw InvalidInputException("invalid calendar date");
    if (tm.tm_year != y - 1900 || tm.tm_mon != m - 1 || tm.tm_mday != d)
        throw InvalidInputException("invalid calendar date (e.g., Feb 30 does not exist)");
    return t;
}

std::time_t Validator::readDate(const std::string& label) {
    while (true) {
        std::string s = prompt(label + " (YYYY-MM-DD)");
        if (s.empty()) {
            tui_.toast("Date required.", Tui::Level::Warning);
            continue;
        }
        try {
            return parseDate(s);
        } catch (const InvalidInputException& e) {
            tui_.toast(e.what(), Tui::Level::Warning);
        }
    }
}

int Validator::parseTime(const std::string& s) {
    std::string trimmed = trim(s);
    if (trimmed.empty())
        throw InvalidInputException("time must be like 1:00 AM or 1:00 PM");

    std::string upper;
    for (char c : trimmed) {
        upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    size_t amPos = upper.find("AM");
    size_t pmPos = upper.find("PM");

    bool isPM = false;
    std::string timePart;

    if (amPos != std::string::npos) {
        timePart = trimmed.substr(0, amPos);
        isPM = false;
    } else if (pmPos != std::string::npos) {
        timePart = trimmed.substr(0, pmPos);
        isPM = true;
    } else {
        throw InvalidInputException("time must include AM or PM (e.g., 1:00 AM)");
    }

    timePart = trim(timePart);

    size_t colonPos = timePart.find(':');
    if (colonPos == std::string::npos)
        throw InvalidInputException("time must include a colon (e.g., 1:00 AM)");

    std::string hourStr = trim(timePart.substr(0, colonPos));
    std::string minStr = trim(timePart.substr(colonPos + 1));

    int h, m;
    try {
        h = std::stoi(hourStr);
        m = std::stoi(minStr);
    } catch (...) {
        throw InvalidInputException("time must be like 1:00 AM or 1:00 PM");
    }

    if (h < 1 || h > 12) throw InvalidInputException("hour must be between 1 and 12");
    if (m < 0 || m > 59) throw InvalidInputException("minute must be between 0 and 59");

    if (isPM) {
        if (h != 12) h += 12;
    } else {
        if (h == 12) h = 0;
    }

    return h * 3600 + m * 60;
}

int Validator::readTimeOfDay(const std::string& label) {
    while (true) {
        std::string s = prompt(label + " (e.g., 1:00 AM)");
        if (s.empty()) {
            tui_.toast("Time required.", Tui::Level::Warning);
            continue;
        }
        try {
            return parseTime(s);
        } catch (const InvalidInputException& e) {
            tui_.toast(e.what(), Tui::Level::Warning);
        }
    }
}

std::string Validator::formatDate(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return buf;
}

std::string Validator::formatDateTime(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min);
    return buf;
}

std::string Validator::formatTime(int secondsOfDay) {
    int h = (secondsOfDay / 3600) % 24;
    int m = (secondsOfDay / 60) % 60;
    std::string period = (h >= 12) ? "PM" : "AM";
    int displayHour = h % 12;
    if (displayHour == 0) displayHour = 12;
    char buf[12];
    std::snprintf(buf, sizeof(buf), "%d:%02d %s", displayHour, m, period.c_str());
    return buf;
}

std::time_t Validator::today() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    return std::mktime(&tm);
}

std::time_t Validator::now() {
    return std::time(nullptr);
}

int Validator::readDuration(const std::string& label, int minMinutes, int maxMinutes) {
    while (true) {
        std::string s = prompt(label + " (e.g., 2:30 or 30)");
        if (s.empty()) {
            tui_.toast("Duration required.", Tui::Level::Warning);
            continue;
        }

        int totalMinutes = 0;
        size_t colonPos = s.find(':');

        if (colonPos != std::string::npos) {
            std::string hourStr = trim(s.substr(0, colonPos));
            std::string minStr = trim(s.substr(colonPos + 1));

            int h, m;
            try {
                h = std::stoi(hourStr);
                m = std::stoi(minStr);
            } catch (...) {
                tui_.toast("Invalid duration format. Use H:MM (e.g., 2:30) or minutes (e.g., 30).",
                           Tui::Level::Warning);
                continue;
            }

            if (h < 0 || h > 23) {
                tui_.toast("Hours must be between 0 and 23.", Tui::Level::Warning);
                continue;
            }
            if (m < 0 || m > 59) {
                tui_.toast("Minutes must be between 0 and 59.", Tui::Level::Warning);
                continue;
            }

            totalMinutes = h * 60 + m;
        } else {
            try {
                std::size_t pos = 0;
                totalMinutes = std::stoi(s, &pos);
                if (pos != s.size()) {
                    tui_.toast("Invalid number.", Tui::Level::Warning);
                    continue;
                }
            } catch (...) {
                tui_.toast("Invalid duration format. Use H:MM (e.g., 2:30) or minutes (e.g., 30).",
                           Tui::Level::Warning);
                continue;
            }
        }

        if (totalMinutes < minMinutes || totalMinutes > maxMinutes) {
            std::ostringstream oss;
            oss << "Duration must be between " << minMinutes << " and " << maxMinutes << " minutes.";
            tui_.toast(oss.str(), Tui::Level::Warning);
            continue;
        }

        return totalMinutes;
    }
}

}
