#include <hms/Core/Csv.h>
#include <hms/Core/Exceptions.h>
#include <hms/Core/Tui.h>
#include <hms/Models/Appointment.h>

#include <sstream>

namespace hms {

std::string toString(AppointmentStatus status) {
    switch (status) {
        case AppointmentStatus::Scheduled: return "Scheduled";
        case AppointmentStatus::Completed: return "Completed";
        case AppointmentStatus::Cancelled: return "Cancelled";
    }
    return "?";
}

const char* statusColor(AppointmentStatus status) {
    (void)status;
    return tui::color::WHITE;
}

std::string Appointment::csvHeader() {
    return "id|patient_id|doctor_id|date|start_min|end_min|status|reason";
}

std::string Appointment::toCSV() const {
    std::ostringstream out;
    out << id_
        << "|" << patientId_
        << "|" << doctorId_
        << "|" << static_cast<long long>(date_)
        << "|" << startMinutes_
        << "|" << endMinutes_
        << "|" << static_cast<int>(status_)
        << "|" << csv::escape(reason_);
    return out.str();
}

Appointment Appointment::fromCSV(const std::string& line) {
    auto fields = csv::split(line);
    if (fields.size() < 8)
        throw FileException("appointment row has too few fields");
    Appointment a;
    try {
        a.id_           = std::stoi(fields[0]);
        a.patientId_    = std::stoi(fields[1]);
        a.doctorId_     = std::stoi(fields[2]);
        a.date_         = static_cast<std::time_t>(std::stoll(fields[3]));
        a.startMinutes_ = std::stoi(fields[4]);
        a.endMinutes_   = std::stoi(fields[5]);
        a.status_       = static_cast<AppointmentStatus>(std::stoi(fields[6]));
        a.reason_       = fields[7];
    } catch (const std::exception& e) {
        throw FileException(std::string("appointment parse: ") + e.what());
    }
    return a;
}

}
