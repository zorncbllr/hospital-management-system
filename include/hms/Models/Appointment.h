#ifndef HMS_MODELS_APPOINTMENT_H
#define HMS_MODELS_APPOINTMENT_H

#include <ctime>
#include <string>

namespace hms {

enum class AppointmentStatus { Scheduled, Completed, Cancelled };

class Appointment {
    int id_;
    int patientId_;
    int doctorId_;
    std::time_t date_;
    int startMinutes_;
    int endMinutes_;
    AppointmentStatus status_;
    std::string reason_;

public:
    Appointment()
        : id_(0), patientId_(0), doctorId_(0), date_(0),
          startMinutes_(0), endMinutes_(0),
          status_(AppointmentStatus::Scheduled) {}

    Appointment(int id, int patientId, int doctorId,
                std::time_t date, int startMinutes, int endMinutes,
                std::string reason)
        : id_(id), patientId_(patientId), doctorId_(doctorId),
          date_(date), startMinutes_(startMinutes), endMinutes_(endMinutes),
          status_(AppointmentStatus::Scheduled),
          reason_(std::move(reason)) {}

    int id() const { return id_; }
    int patientId() const { return patientId_; }
    int doctorId() const { return doctorId_; }
    std::time_t date() const { return date_; }
    int startMinutes() const { return startMinutes_; }
    int endMinutes() const { return endMinutes_; }
    AppointmentStatus status() const { return status_; }
    const std::string& reason() const { return reason_; }

    void setId(int value) { id_ = value; }
    void setStatus(AppointmentStatus value) { status_ = value; }

    bool overlapsWith(const Appointment& other) const {
        if (date_ != other.date_) return false;
        if (doctorId_ != other.doctorId_) return false;
        if (status_ == AppointmentStatus::Cancelled) return false;
        if (other.status_ == AppointmentStatus::Cancelled) return false;
        return startMinutes_ < other.endMinutes_ &&
               other.startMinutes_ < endMinutes_;
    }

    static std::string csvHeader();
    std::string toCSV() const;
    static Appointment fromCSV(const std::string& line);
};

std::string toString(AppointmentStatus status);
const char* statusColor(AppointmentStatus status);

}

#endif
