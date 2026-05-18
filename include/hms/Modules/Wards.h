#ifndef HMS_MODULES_WARDS_H
#define HMS_MODULES_WARDS_H

#include <hms/Modules/Module.h>

#include <string>

namespace hms {

class WardsModule : public Module {
public:
    using Module::Module;

    void run() override;
    char        menuKey()   const override { return '5'; }
    std::string menuLabel() const override { return "Ward & Beds"; }
    std::string menuHint()  const override { return "admissions and bed management"; }

private:
    void showAllBeds();
    void admitToBed();
    void dischargeFromBed();
    void showAdmittedPatients();
};

}

#endif
