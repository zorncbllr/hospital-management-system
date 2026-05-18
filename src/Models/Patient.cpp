#include <hms/Core/Csv.h>
#include <hms/Core/Exceptions.h>
#include <hms/Core/Tui.h>
#include <hms/Models/Patient.h>

#include <sstream>

namespace hms {

const char* severityLabel(int level) {
    switch (level) {
        case SEVERITY_CRITICAL:    return "5-CRITICAL";
        case SEVERITY_EMERGENT:    return "4-EMERGENT";
        case SEVERITY_URGENT:      return "3-URGENT";
        case SEVERITY_LESS_URGENT: return "2-LESS URGENT";
        case SEVERITY_NON_URGENT:  return "1-NON-URGENT";
        default:                   return "?";
    }
}

const char* severityColor(int level) {
    (void)level;
    return Tui::Color::WHITE;
}

std::string Patient::summary() const {
    std::ostringstream out;
    out << "#" << id_ << " " << name_
        << " (" << age_ << sex_ << ")"
        << " severity=" << severity_;
    return out.str();
}

std::string Patient::csvHeader() {
    return "id|name|age|sex|contact|address|severity|arrival|"
           "senior|pwd|philhealth|promo|bed_id|doctor_id";
}

std::string Patient::toCSV() const {
    std::ostringstream out;
    out << id_
        << "|" << CsvCodec::escape(name_)
        << "|" << age_
        << "|" << sex_
        << "|" << CsvCodec::escape(contact_)
        << "|" << CsvCodec::escape(address_)
        << "|" << severity_
        << "|" << static_cast<long long>(arrival_)
        << "|" << (senior_ ? 1 : 0)
        << "|" << (pwd_ ? 1 : 0)
        << "|" << (philhealth_ ? 1 : 0)
        << "|" << CsvCodec::escape(promoCode_)
        << "|" << bedId_
        << "|" << doctorId_;
    return out.str();
}

Patient Patient::fromCSV(const std::string& line) {
    auto fields = CsvCodec::split(line);
    if (fields.size() < 14)
        throw FileException("patient row has too few fields");
    Patient p;
    try {
        p.id_         = std::stoi(fields[0]);
        p.name_       = fields[1];
        p.age_        = std::stoi(fields[2]);
        p.sex_        = fields[3].empty() ? 'M' : fields[3][0];
        p.contact_    = fields[4];
        p.address_    = fields[5];
        p.severity_   = std::stoi(fields[6]);
        p.arrival_    = static_cast<std::time_t>(std::stoll(fields[7]));
        p.senior_     = std::stoi(fields[8]) != 0;
        p.pwd_        = std::stoi(fields[9]) != 0;
        p.philhealth_ = std::stoi(fields[10]) != 0;
        p.promoCode_  = fields[11];
        p.bedId_      = std::stoi(fields[12]);
        p.doctorId_   = std::stoi(fields[13]);
    } catch (const std::exception& e) {
        throw FileException(std::string("patient parse: ") + e.what());
    }
    return p;
}

}
