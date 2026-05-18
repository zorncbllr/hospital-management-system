#include <hms/Core/Csv.h>
#include <hms/Core/Exceptions.h>
#include <hms/Models/Bed.h>

#include <sstream>

namespace hms {

std::string Bed::csvHeader() {
    return "id|ward|occupant_id|daily_rate|admit_date";
}

std::string Bed::toCSV() const {
    std::ostringstream out;
    out << id_
        << "|" << CsvCodec::escape(ward_)
        << "|" << occupantId_
        << "|" << dailyRate_
        << "|" << static_cast<long long>(admitDate_);
    return out.str();
}

Bed Bed::fromCSV(const std::string& line) {
    auto fields = CsvCodec::split(line);
    if (fields.size() < 5)
        throw FileException("bed row has too few fields");
    Bed b;
    try {
        b.id_         = std::stoi(fields[0]);
        b.ward_       = fields[1];
        b.occupantId_ = std::stoi(fields[2]);
        b.dailyRate_  = std::stod(fields[3]);
        b.admitDate_  = static_cast<std::time_t>(std::stoll(fields[4]));
    } catch (const std::exception& e) {
        throw FileException(std::string("bed parse: ") + e.what());
    }
    return b;
}

}
