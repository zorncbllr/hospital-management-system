#ifndef HMS_MODELS_PATIENT_H
#define HMS_MODELS_PATIENT_H

#include <hms/Models/Person.h>

#include <ctime>
#include <string>

namespace hms {

constexpr int SEVERITY_CRITICAL    = 5;
constexpr int SEVERITY_EMERGENT    = 4;
constexpr int SEVERITY_URGENT      = 3;
constexpr int SEVERITY_LESS_URGENT = 2;
constexpr int SEVERITY_NON_URGENT  = 1;

class Patient : public Person {
    int severity_;
    std::time_t arrival_;
    bool senior_;
    bool pwd_;
    bool philhealth_;
    std::string promoCode_;
    int bedId_;
    int doctorId_;

public:
    Patient()
        : severity_(SEVERITY_NON_URGENT), arrival_(0),
          senior_(false), pwd_(false), philhealth_(false),
          bedId_(-1), doctorId_(-1) {}

    Patient(int id, std::string name, int age, char sex,
            std::string contact, std::string address)
        : Person(id, std::move(name), age, sex,
                 std::move(contact), std::move(address)),
          severity_(SEVERITY_NON_URGENT), arrival_(0),
          senior_(false), pwd_(false), philhealth_(false),
          bedId_(-1), doctorId_(-1) {}

    int severity() const { return severity_; }
    std::time_t arrival() const { return arrival_; }
    bool isSenior() const { return senior_; }
    bool isPWD() const { return pwd_; }
    bool hasPhilHealth() const { return philhealth_; }
    const std::string& promoCode() const { return promoCode_; }
    int bedId() const { return bedId_; }
    int doctorId() const { return doctorId_; }

    bool isAdmitted() const { return bedId_ >= 0; }
    bool hasDoctor() const { return doctorId_ >= 0; }

    void setSeverity(int level) { severity_ = level; }
    void setArrival(std::time_t when) { arrival_ = when; }
    void setSenior(bool yes) { senior_ = yes; }
    void setPWD(bool yes) { pwd_ = yes; }
    void setPhilHealth(bool yes) { philhealth_ = yes; }
    void setPromoCode(const std::string& code) { promoCode_ = code; }
    void setBedId(int id) { bedId_ = id; }
    void setDoctorId(int id) { doctorId_ = id; }

    std::string role() const override { return "Patient"; }
    std::string summary() const override;

    static std::string csvHeader();
    std::string toCSV() const;
    static Patient fromCSV(const std::string& line);
};

const char* severityLabel(int level);
const char* severityColor(int level);

}

#endif
