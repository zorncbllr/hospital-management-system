#ifndef HMS_MODULES_BILLING_H
#define HMS_MODULES_BILLING_H

#include <hms/Modules/Module.h>

#include <string>
#include <vector>

namespace hms {

class Patient;
class Bill;
enum class BillStatus;

class BillingModule : public Module {
public:
    using Module::Module;

    void run() override;
    char        menuKey()   const override { return '7'; }
    std::string menuLabel() const override { return "Billing"; }
    std::string menuHint()  const override { return "invoices and payments"; }

private:
    static constexpr double DISCOUNT_CAP_PERCENT = 0.50;

    struct DiscountOption {
        std::string code;
        std::string label;
        std::string group;
        double percent;
    };
    struct DiscountPlan {
        std::vector<DiscountOption> picked;
        double totalAmount = 0.0;
    };

    static std::string                 billStatusName(BillStatus status);
    static void                        logAudit(const std::string& dataDir,
                                                const std::string& action,
                                                const std::string& details);
    static std::vector<DiscountOption> eligibleDiscounts(const Patient& patient,
                                                         double subtotal);
    static DiscountPlan                optimalDiscountPlan(
        const std::vector<DiscountOption>& options, double subtotal);

    void addRoomCharges(const Patient& patient, Bill& bill);
    void addDoctorFee(const Patient& patient, Bill& bill);
    void addManualItems(Bill& bill);
    void printReceipt(const Patient& patient, const Bill& bill,
                      const DiscountPlan& plan);

    void newBill();
    void viewBill();
    void listBills();
    void updateBillStatus();
};

}

#endif
