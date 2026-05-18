#ifndef HMS_MODULES_REPORTS_H
#define HMS_MODULES_REPORTS_H

#include <hms/Modules/Module.h>

#include <ctime>
#include <string>

namespace hms {

class ReportsModule : public Module {
public:
    using Module::Module;

    void run() override;
    char        menuKey()   const override { return '9'; }
    std::string menuLabel() const override { return "Reports"; }
    std::string menuHint()  const override { return "hospital-wide analytics"; }

private:
    static std::string percent(double ratio);
    static bool        sameDay(std::time_t a, std::time_t b);

    void dailySummary();
    void bedOccupancy();
    void doctorWorkload();
    void revenueAndDiscounts();
    void exportAll();
};

}

#endif
