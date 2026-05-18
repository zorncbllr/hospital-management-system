#ifndef HMS_MODELS_BED_H
#define HMS_MODELS_BED_H

#include <ctime>
#include <string>

namespace hms {

class Bed {
    int id_;
    std::string ward_;
    int occupantId_;
    double dailyRate_;
    std::time_t admitDate_;

public:
    Bed() : id_(0), occupantId_(-1), dailyRate_(0.0), admitDate_(0) {}

    Bed(int id, std::string ward, double dailyRate)
        : id_(id), ward_(std::move(ward)), occupantId_(-1),
          dailyRate_(dailyRate), admitDate_(0) {}

    int id() const { return id_; }
    const std::string& ward() const { return ward_; }
    int occupantId() const { return occupantId_; }
    double dailyRate() const { return dailyRate_; }
    std::time_t admitDate() const { return admitDate_; }

    bool isFree() const { return occupantId_ < 0; }

    void admit(int patientId, std::time_t when) {
        occupantId_ = patientId;
        admitDate_  = when;
    }
    void discharge() {
        occupantId_ = -1;
        admitDate_  = 0;
    }

    int daysOccupied(std::time_t referenceDay) const {
        if (isFree()) return 0;
        int days = static_cast<int>((referenceDay - admitDate_) / 86400);
        return days < 1 ? 1 : days;
    }

    static std::string csvHeader();
    std::string toCSV() const;
    static Bed fromCSV(const std::string& line);
};

}

#endif
