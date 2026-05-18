#ifndef HMS_MODELS_DOCTOR_H
#define HMS_MODELS_DOCTOR_H

#include <hms/Models/Person.h>

#include <string>

namespace hms {

class Doctor : public Person {
    std::string specialty_;
    std::string room_;
    double consultFee_;
    int dailyAppointmentLimit_;

public:
    Doctor() : consultFee_(0.0), dailyAppointmentLimit_(8) {}
    Doctor(int id, std::string name, int age, char sex,
           std::string contact, std::string address,
           std::string specialty, std::string room,
           double consultFee, int dailyLimit)
        : Person(id, std::move(name), age, sex,
                 std::move(contact), std::move(address)),
          specialty_(std::move(specialty)),
          room_(std::move(room)),
          consultFee_(consultFee),
          dailyAppointmentLimit_(dailyLimit) {}

    const std::string& specialty() const { return specialty_; }
    const std::string& room() const { return room_; }
    double consultFee() const { return consultFee_; }
    int dailyAppointmentLimit() const { return dailyAppointmentLimit_; }

    void setSpecialty(const std::string& value) { specialty_ = value; }
    void setRoom(const std::string& value) { room_ = value; }
    void setConsultFee(double value) { consultFee_ = value; }
    void setDailyAppointmentLimit(int value) { dailyAppointmentLimit_ = value; }

    std::string role() const override { return "Doctor"; }
    std::string summary() const override;

    static std::string csvHeader();
    std::string toCSV() const;
    static Doctor fromCSV(const std::string& line);
};

}

#endif
