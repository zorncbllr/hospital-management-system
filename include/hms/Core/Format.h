#ifndef HMS_CORE_FORMAT_H
#define HMS_CORE_FORMAT_H

#include <iomanip>
#include <sstream>
#include <string>

namespace hms {

class Formatter {
public:
    Formatter() = delete;

    static std::string money(double value) {
        std::ostringstream out;
        out << "P" << std::fixed << std::setprecision(2) << value;
        return out.str();
    }
};

}

#endif
