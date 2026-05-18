#ifndef HMS_MODULES_PATIENTS_H
#define HMS_MODULES_PATIENTS_H

#include <hms/Modules/Module.h>

#include <string>
#include <vector>

namespace hms {

class Patient;

class PatientsModule : public Module {
public:
    using Module::Module;

    void run() override;
    char        menuKey()   const override { return '3'; }
    std::string menuLabel() const override { return "Patient Records"; }
    std::string menuHint()  const override { return "register, edit, search patients"; }

private:
    static void                 logAudit(const std::string& dataDir,
                                         const std::string& action,
                                         const std::string& details);
    static std::vector<Patient> filterPatients(const std::vector<Patient>& all,
                                               const std::string& query);

    void showPatientList(const std::vector<Patient>& patients,
                         const std::string& title);
    void addPatient();
    void editPatient();
    void deletePatient();
    void listPatients();
};

}

#endif
