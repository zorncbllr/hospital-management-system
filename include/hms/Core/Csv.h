#ifndef HMS_CORE_CSV_H
#define HMS_CORE_CSV_H

#include <string>
#include <vector>

namespace hms::csv {

inline std::vector<std::string> split(const std::string& s, char delim = '|') {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == delim) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

inline std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '|')                       out += '/';
        else if (c == '\n' || c == '\r')    out += ' ';
        else                                out += c;
    }
    return out;
}

}

#endif
