#include <hms/Core/Csv.h>
#include <hms/Core/Exceptions.h>
#include <hms/Models/Doctor.h>

#include <sstream>

namespace hms {

std::string Doctor::summary() const {
    std::ostringstream out;
    out << "Dr. " << name_ << " — " << specialty_
        << " (room " << room_ << ")";
    return out.str();
}

std::string Doctor::csvHeader() {
    return "id|name|age|sex|contact|address|specialty|room|fee|daily_limit";
}

std::string Doctor::toCSV() const {
    std::ostringstream out;
    out << id_
        << "|" << csv::escape(name_)
        << "|" << age_
        << "|" << sex_
        << "|" << csv::escape(contact_)
        << "|" << csv::escape(address_)
        << "|" << csv::escape(specialty_)
        << "|" << csv::escape(room_)
        << "|" << consultFee_
        << "|" << dailyAppointmentLimit_;
    return out.str();
}

Doctor Doctor::fromCSV(const std::string& line) {
    auto fields = csv::split(line);
    if (fields.size() < 10)
        throw FileException("doctor row has too few fields");
    Doctor d;
    try {
        d.id_                      = std::stoi(fields[0]);
        d.name_                    = fields[1];
        d.age_                     = std::stoi(fields[2]);
        d.sex_                     = fields[3].empty() ? 'M' : fields[3][0];
        d.contact_                 = fields[4];
        d.address_                 = fields[5];
        d.specialty_               = fields[6];
        d.room_                    = fields[7];
        d.consultFee_              = std::stod(fields[8]);
        d.dailyAppointmentLimit_   = std::stoi(fields[9]);
    } catch (const std::exception& e) {
        throw FileException(std::string("doctor parse: ") + e.what());
    }
    return d;
}

}
