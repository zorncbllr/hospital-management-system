#include <hms/Core/Exceptions.h>
#include <hms/Core/Format.h>
#include <hms/Core/Tui.h>
#include <hms/Core/Validation.h>
#include <hms/Hospital.h>
#include <hms/Models/Bed.h>
#include <hms/Models/Bill.h>
#include <hms/Models/Doctor.h>
#include <hms/Models/Patient.h>
#include <hms/Modules/Billing.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace hms {


std::string BillingModule::billStatusName(BillStatus status) {
    switch (status) {
        case BillStatus::Unpaid:  return "Unpaid";
        case BillStatus::Paid:    return "Paid";
        case BillStatus::Voided:  return "Voided";
    }
    return "Unpaid";
}

void BillingModule::logAudit(const std::string& dataDir, const std::string& action, const std::string& details) {
    std::ofstream log(dataDir + "/audit.log", std::ios::app);
    if (log.is_open()) {
        std::time_t now = Validator::now();
        log << Validator::formatDateTime(now) << " | "
            << action << " | " << details << "\n";
    }
}

std::vector<BillingModule::DiscountOption> BillingModule::eligibleDiscounts(
    const Patient& patient, double subtotal) {
    std::vector<DiscountOption> options;
    if (patient.hasPhilHealth()) {
        options.push_back({"PHILHEALTH", "PhilHealth coverage",
                           "Government", 0.20});
    }
    if (patient.isSenior()) {
        options.push_back({"SENIOR", "Senior citizen discount",
                           "Government", 0.20});
    }
    if (patient.isPWD()) {
        options.push_back({"PWD", "PWD discount",
                           "Government", 0.20});
    }
    options.push_back({"PROMO10", "Hospital welcome promo (10%)",
                       "Promo", 0.10});
    if (patient.promoCode() == "HEALTH15") {
        options.push_back({"HEALTH15", "HEALTH15 promo code (15%)",
                           "Promo", 0.15});
    }
    options.push_back({"LOYALTY", "Loyalty 5%",
                       "Loyalty", 0.05});

    if (subtotal <= 0.0) options.clear();
    return options;
}

BillingModule::DiscountPlan BillingModule::optimalDiscountPlan(
    const std::vector<DiscountOption>& options,
    double subtotal) {

    DiscountPlan plan;
    if (options.empty() || subtotal <= 0.0) return plan;

    std::vector<std::string> groupOrder;
    std::vector<std::vector<int>> optionsByGroup;
    auto groupIndex = [&](const std::string& name) {
        for (std::size_t i = 0; i < groupOrder.size(); ++i)
            if (groupOrder[i] == name) return static_cast<int>(i);
        groupOrder.push_back(name);
        optionsByGroup.push_back({});
        return static_cast<int>(groupOrder.size() - 1);
    };
    for (std::size_t i = 0; i < options.size(); ++i) {
        int g = groupIndex(options[i].group);
        optionsByGroup[g].push_back(static_cast<int>(i));
    }

    const int groupCount = static_cast<int>(groupOrder.size());
    const int weightCap =
        static_cast<int>(subtotal * DISCOUNT_CAP_PERCENT);

    std::vector<int> weight(options.size());
    for (std::size_t i = 0; i < options.size(); ++i) {
        weight[i] = static_cast<int>(options[i].percent * subtotal);
    }

    std::vector<std::vector<int>> dp(
        groupCount + 1, std::vector<int>(weightCap + 1, 0));
    std::vector<std::vector<int>> choice(
        groupCount + 1, std::vector<int>(weightCap + 1, -1));

    for (int g = 1; g <= groupCount; ++g) {
        for (int w = 0; w <= weightCap; ++w) {
            dp[g][w] = dp[g - 1][w];
            choice[g][w] = -1;
            for (int optionIndex : optionsByGroup[g - 1]) {
                int weightOfOption = weight[optionIndex];
                int valueOfOption = weight[optionIndex];
                if (weightOfOption > w) continue;
                int candidate = dp[g - 1][w - weightOfOption] + valueOfOption;
                if (candidate > dp[g][w]) {
                    dp[g][w] = candidate;
                    choice[g][w] = optionIndex;
                }
            }
        }
    }

    int g = groupCount;
    int w = weightCap;
    while (g > 0) {
        int picked = choice[g][w];
        if (picked >= 0) {
            plan.picked.push_back(options[picked]);
            w -= weight[picked];
        }
        --g;
    }
    plan.totalAmount = dp[groupCount][weightCap];
    return plan;
}

void BillingModule::addRoomCharges(const Patient& patient,
                    Bill& bill) {
    if (!patient.isAdmitted()) return;
    std::time_t today = Validator::today();
    for (const auto& bed : hospital_.beds()) {
        if (bed.occupantId() != patient.id()) continue;
        int days = bed.daysOccupied(today);
        BillItem item;
        item.category    = ChargeCategory::Room;
        item.description = bed.ward() + " ward, bed #" + std::to_string(bed.id());
        item.quantity    = days;
        item.unitPrice   = bed.dailyRate();
        bill.addItem(item);
        return;
    }
}

void BillingModule::addDoctorFee(const Patient& patient,
                  Bill& bill) {
    if (!patient.hasDoctor()) return;
    for (const auto& doctor : hospital_.doctors()) {
        if (doctor.id() != patient.doctorId()) continue;
        BillItem item;
        item.category    = ChargeCategory::DoctorFee;
        item.description = "Dr. " + doctor.name() + " — " + doctor.specialty();
        item.quantity    = 1;
        item.unitPrice   = doctor.consultFee();
        bill.addItem(item);
        return;
    }
}

void BillingModule::addManualItems(Bill& bill) {
    std::cout << "\n  " << Tui::Color::DIM
              << "Add procedures / extra items. Enter 0 quantity to stop."
              << Tui::Color::RESET << "\n";
    while (true) {
        std::string description = validation_.readLine(
            "Description (blank to stop)", true);
        if (description.empty()) break;
        int qty = validation_.readInt("Quantity", 0, 1000);
        if (qty == 0) break;
        double price = validation_.readDouble("Unit price",
                                              0.0, 1000000.0);
        BillItem item;
        item.category    = ChargeCategory::Procedure;
        item.description = description;
        item.quantity    = qty;
        item.unitPrice   = price;
        bill.addItem(item);
    }
}

void BillingModule::printReceipt(const Patient& patient,
                  const Bill& bill, const DiscountPlan& plan) {
    tui_.clearScreen();

    std::vector<std::string> headers{ "Category", "Description", "Qty", "Unit", "Amount" };
    std::vector<std::vector<std::string>> rows;
    for (const auto& item : bill.items()) {
        rows.push_back({
            toString(item.category),
            item.description,
            std::to_string(item.quantity),
            Formatter::money(item.unitPrice),
            Formatter::money(item.subtotal()),
        });
    }
    std::string billHeader = "Bill #" + std::to_string(bill.id()) +
                             "   " + Validator::formatDate(bill.date()) +
                             "   " + patient.name() +
                             "   [" + billStatusName(bill.status()) + "]";
    int bw = tui_.bannerOpen("OFFICIAL RECEIPT", "optimal discount combination applied", {},
                             tui_.tableBoxWidth(headers, rows, billHeader));
    tui_.tableInBox(bw, headers, rows, billHeader);

    std::vector<std::string> sHeaders{"Description", "Amount"};
    std::vector<std::vector<std::string>> sRows;
    sRows.push_back({"Subtotal", Formatter::money(bill.subtotal())});

    if (!plan.picked.empty()) {
        sRows.push_back({"", ""});
        sRows.push_back({"Discounts applied", ""});
        for (const auto& option : plan.picked) {
            double amount = patient.id() && bill.subtotal() > 0
                ? bill.subtotal() * option.percent : 0.0;
            sRows.push_back({
                "  - " + option.label + " (" + option.group + ", " +
                    std::to_string(static_cast<int>(option.percent * 100)) + "%)",
                "-" + Formatter::money(amount),
            });
        }
        sRows.push_back({"Total discount", "-" + Formatter::money(plan.totalAmount)});
    } else {
        sRows.push_back({"", ""});
        sRows.push_back({"No discounts available", ""});
    }

    sRows.push_back({"", ""});
    sRows.push_back({std::string(Tui::Color::BOLD) + "AMOUNT DUE" + Tui::Color::RESET,
                     std::string(Tui::Color::BOLD) + Formatter::money(bill.total()) + Tui::Color::RESET});

    int sw = tui_.bannerOpen("OFFICIAL RECEIPT", "optimal discount combination applied", {},
                             tui_.tableBoxWidth(sHeaders, sRows, billHeader));
    tui_.tableInBox(sw, sHeaders, sRows, billHeader);
    std::cout << "\n";
}

void BillingModule::newBill() {
    if (hospital_.patients().empty())
        throw NotFoundException("no patients in the system");

    tui_.clearScreen();
    tui_.banner("CREATE BILL", "itemise charges and apply best discount", {"Home", "Billing", "New Bill"});
    std::cout << "\n";

    std::vector<std::string> pHeaders{"ID", "Name", "Age", "Sex", "Status"};
    std::vector<std::vector<std::string>> pRows;
    for (const auto& p : hospital_.patients()) {
        std::string sx = (p.sex() == 'M' || p.sex() == 'm') ? "Male" : "Female";
        std::string st = p.isAdmitted() ? "admitted" : p.hasDoctor() ? "with doctor" : "waiting";
        pRows.push_back({
            std::to_string(p.id()), p.name(), std::to_string(p.age()), sx, st,
        });
    }
    int pw = tui_.bannerOpen("SELECT PATIENT", "",
                             {"Home", "Billing", "New Bill"},
                             tui_.tableBoxWidth(pHeaders, pRows));
    tui_.tableInBox(pw, pHeaders, pRows);
    std::cout << "\n";
    int patientId = validation_.readInt("Patient id", 1, 1000000);
    Patient* patient = hospital_.findPatient(patientId);
    if (!patient) throw NotFoundException("patient #" + std::to_string(patientId));

    for (const auto& existing : hospital_.bills()) {
        if (existing.patientId() == patientId && existing.status() == BillStatus::Unpaid) {
            tui_.toast("This patient already has an unpaid bill (bill #" +
                       std::to_string(existing.id()) + ", " +
                       Formatter::money(existing.total()) + "). "
                       "Please settle or void it before creating a new one.",
                       Tui::Level::Warning);
            tui_.pause();
            return;
        }
    }

    Bill bill(hospital_.nextBillId(), patientId, Validator::now());

    addRoomCharges(*patient, bill);
    addDoctorFee(*patient, bill);
    addManualItems(bill);

    if (bill.subtotal() <= 0.0) {
        tui_.toast("No charges entered. Bill not created.",
                   Tui::Level::Warning);
        tui_.pause();
        return;
    }

    auto options = eligibleDiscounts(*patient, bill.subtotal());
    DiscountPlan plan = optimalDiscountPlan(options, bill.subtotal());

    std::string appliedNames;
    for (std::size_t i = 0; i < plan.picked.size(); ++i) {
        if (i > 0) appliedNames += "+";
        appliedNames += plan.picked[i].code;
    }
    bill.applyDiscount(appliedNames, plan.totalAmount);

    printReceipt(*patient, bill, plan);

    if (!tui_.confirm("Save this bill?")) {
        tui_.toast("Discarded.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    hospital_.bills().push_back(bill);
    hospital_.saveAll();
    logAudit(hospital_.dataDir(), "BILL_CREATED",
             "id=" + std::to_string(bill.id()) + " patient=" + patient->name() +
             " total=" + std::to_string(bill.total()));
    tui_.toast("Bill saved.", Tui::Level::Success);
    tui_.pause();
}

void BillingModule::viewBill() {
    tui_.clearScreen();
    tui_.banner("VIEW BILL", "", {"Home", "Billing", "View"});
    std::cout << "\n";

    if (hospital_.bills().empty()) {
        tui_.toast("No bills in the system.", Tui::Level::Info);
        tui_.pause();
        return;
    }
    std::vector<std::string> bHeaders{"ID", "Date", "Patient", "Total"};
    std::vector<std::vector<std::string>> bRows;
    for (const auto& b : hospital_.bills()) {
        Patient* p = hospital_.findPatient(b.patientId());
        std::ostringstream totalStr;
        totalStr << "P" << std::fixed << std::setprecision(2) << b.total();
        bRows.push_back({
            std::to_string(b.id()),
            Validator::formatDate(b.date()),
            p ? p->name() : "#" + std::to_string(b.patientId()),
            totalStr.str(),
        });
    }
    int bw = tui_.bannerOpen("SELECT BILL", "",
                             {"Home", "Billing", "View"},
                             tui_.tableBoxWidth(bHeaders, bRows));
    tui_.tableInBox(bw, bHeaders, bRows);
    std::cout << "\n";
    int billId = validation_.readInt("Bill id", 1, 1000000);
    for (const auto& bill : hospital_.bills()) {
        if (bill.id() != billId) continue;
        Patient* patient = hospital_.findPatient(bill.patientId());
        if (!patient) {
            tui_.toast("Patient record for this bill has been deleted. Showing bill details without patient info.",
                       Tui::Level::Warning);
            tui_.pause();
            return;
        }

        auto options = eligibleDiscounts(*patient, bill.subtotal());
        DiscountPlan plan = optimalDiscountPlan(options, bill.subtotal());
        if (plan.totalAmount != bill.discountAmount()) {
            plan.totalAmount = bill.discountAmount();
        }
        printReceipt(*patient, bill, plan);
        tui_.pause();
        return;
    }
    throw NotFoundException("bill #" + std::to_string(billId));
}

void BillingModule::listBills() {
    tui_.clearScreen();

    std::vector<std::string> headers{ "Bill", "Date", "Patient", "Subtotal", "Discount", "Total", "Status" };
    std::vector<std::vector<std::string>> rows;
    for (const auto& bill : hospital_.bills()) {
        Patient* patient = hospital_.findPatient(bill.patientId());
        rows.push_back({
            "#" + std::to_string(bill.id()),
            Validator::formatDate(bill.date()),
            patient ? patient->name() : ("#" + std::to_string(bill.patientId())),
            Formatter::money(bill.subtotal()),
            Formatter::money(bill.discountAmount()),
            Formatter::money(bill.total()),
            billStatusName(bill.status()),
        });
    }
    int bw = tui_.bannerOpen("ALL BILLS", "", {"Home", "Billing", "List"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";
    tui_.pause();
}

void BillingModule::updateBillStatus() {
    tui_.clearScreen();
    tui_.banner("UPDATE BILL STATUS", "", {"Home", "Billing", "Update Status"});
    std::cout << "\n";

    if (hospital_.bills().empty()) {
        tui_.toast("No bills in the system.", Tui::Level::Info);
        tui_.pause();
        return;
    }
    std::vector<std::string> bHeaders{"ID", "Date", "Patient", "Total", "Status"};
    std::vector<std::vector<std::string>> bRows;
    for (const auto& b : hospital_.bills()) {
        Patient* p = hospital_.findPatient(b.patientId());
        bRows.push_back({
            std::to_string(b.id()),
            Validator::formatDate(b.date()),
            p ? p->name() : "#" + std::to_string(b.patientId()),
            Formatter::money(b.total()),
            billStatusName(b.status()),
        });
    }
    int bw = tui_.bannerOpen("SELECT BILL", "",
                             {"Home", "Billing", "Update Status"},
                             tui_.tableBoxWidth(bHeaders, bRows));
    tui_.tableInBox(bw, bHeaders, bRows);
    std::cout << "\n";
    int billId = validation_.readInt("Bill id", 1, 1000000);
    Bill* bill = nullptr;
    for (auto& b : hospital_.bills()) {
        if (b.id() == billId) { bill = &b; break; }
    }
    if (!bill) throw NotFoundException("bill #" + std::to_string(billId));

    char choice = tui_.menu(
        "SET STATUS FOR BILL #" + std::to_string(bill->id()),
        {"Home", "Billing", "Update Status"},
        {
            { '1', "Mark as Paid",    "payment received" },
            { '2', "Mark as Voided",  "cancel this bill" },
            { '3', "Mark as Unpaid",  "revert to unpaid" },
            { 'B', "Back",            "no changes" },
        });
    if (choice == 'B') return;

    BillStatus newStatus;
    if (choice == '1')      newStatus = BillStatus::Paid;
    else if (choice == '2') newStatus = BillStatus::Voided;
    else                    newStatus = BillStatus::Unpaid;

    if (bill->status() == newStatus) {
        tui_.toast("Bill is already " + billStatusName(newStatus) + ".", Tui::Level::Info);
        tui_.pause();
        return;
    }

    std::cout << "\n  Changing bill #" << bill->id() << " from "
              << billStatusName(bill->status()) << " to " << billStatusName(newStatus) << "\n\n";
    if (!tui_.confirm("Confirm?")) {
        tui_.toast("No changes.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    bill->setStatus(newStatus);
    hospital_.saveAll();
    logAudit(hospital_.dataDir(), "BILL_STATUS_UPDATED",
             "id=" + std::to_string(bill->id()) + " from=" + billStatusName(bill->status()) +
             " to=" + billStatusName(newStatus));
    tui_.toast("Bill #" + std::to_string(bill->id()) + " marked as " +
               billStatusName(newStatus) + ".", Tui::Level::Success);
    tui_.pause();
}


void BillingModule::run() {
    while (true) {
        char choice = tui_.menu(
            "BILLING",
            {"Home", "Billing"},
            {
                { '1', "Create new bill",   "itemise + apply discounts" },
                { '2', "View bill",         "show one bill in detail" },
                { '3', "List all bills",    "summary table" },
                { '4', "Update bill status", "mark as paid/voided" },
                { 'B', "Back",              "return to main menu" },
            });
        switch (choice) {
            case '1': newBill();        break;
            case '2': viewBill();       break;
            case '3': listBills();      break;
            case '4': updateBillStatus(); break;
            case 'B': return;
        }
    }
}

}
