#ifndef HMS_MODULES_PHARMACY_H
#define HMS_MODULES_PHARMACY_H

#include <hms/Modules/Module.h>

#include <string>
#include <vector>

namespace hms {

class Medicine;

class PharmacyModule : public Module {
public:
    using Module::Module;

    void run() override;
    char        menuKey()   const override { return '6'; }
    std::string menuLabel() const override { return "Pharmacy"; }
    std::string menuHint()  const override { return "medicines and inventory"; }

private:
    struct ExpiringFirst {
        bool operator()(const Medicine* a, const Medicine* b) const;
    };

    static void                   logAudit(const std::string& dataDir,
                                           const std::string& action,
                                           const std::string& details);
    static std::vector<Medicine>  filterMedicines(const std::vector<Medicine>& all,
                                                  const std::string& query);
    static std::vector<Medicine*> sortedByExpiry(std::vector<Medicine>& medicines);

    void listMedicines();
    void editMedicine();
    void deleteMedicine();
    void addMedicine();
    void restockMedicine();
    void dispense();
    void lowStockAlerts();
    void nearExpiryAlerts();
};

}

#endif
