#include <hms/Core/Csv.h>
#include <hms/Core/Exceptions.h>
#include <hms/Models/Medicine.h>

#include <sstream>

namespace hms {

std::string Medicine::csvHeader() {
    return "sku|name|stock|reorder|price|expiry";
}

std::string Medicine::toCSV() const {
    std::ostringstream out;
    out << CsvCodec::escape(sku_)
        << "|" << CsvCodec::escape(name_)
        << "|" << stock_
        << "|" << reorderLevel_
        << "|" << unitPrice_
        << "|" << static_cast<long long>(expiry_);
    return out.str();
}

Medicine Medicine::fromCSV(const std::string& line) {
    auto fields = CsvCodec::split(line);
    if (fields.size() < 6)
        throw FileException("medicine row has too few fields");
    Medicine m;
    try {
        m.sku_          = fields[0];
        m.name_         = fields[1];
        m.stock_        = std::stoi(fields[2]);
        m.reorderLevel_ = std::stoi(fields[3]);
        m.unitPrice_    = std::stod(fields[4]);
        m.expiry_       = static_cast<std::time_t>(std::stoll(fields[5]));
    } catch (const std::exception& e) {
        throw FileException(std::string("medicine parse: ") + e.what());
    }
    return m;
}

}
