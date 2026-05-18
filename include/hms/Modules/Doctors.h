#ifndef HMS_MODULES_DOCTORS_H
#define HMS_MODULES_DOCTORS_H

#include <hms/Modules/Module.h>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace hms {

class Doctor;

class DoctorsModule : public Module {
public:
    using Module::Module;

    void run() override;
    char        menuKey()   const override { return '4'; }
    std::string menuLabel() const override { return "Doctor Records"; }
    std::string menuHint()  const override { return "manage staff and schedules"; }

private:
    struct HungarianResult {
        std::vector<int> assignment;
        int totalCost;
    };

    static void                logAudit(const std::string& dataDir,
                                        const std::string& action,
                                        const std::string& details);
    static std::vector<Doctor> filterDoctors(const std::vector<Doctor>& all,
                                             const std::string& query);
    static HungarianResult     solveHungarian(const std::vector<std::vector<int>>& cost);

    int  doctorWorkload(int doctorId);
    void showDoctorList(const std::vector<Doctor>& doctors,
                        const std::string& title);
    void addDoctor();
    void editDoctor();
    void deleteDoctor();
    void listDoctors();
    void autoAssignDoctors();

    static constexpr int REGISTRATION_COOLDOWN_SECONDS = 2;
    std::optional<std::chrono::steady_clock::time_point> lastDoctorRegistration_;
};

}

#endif
