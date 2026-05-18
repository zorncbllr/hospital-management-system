#include <hms/Core/Exceptions.h>
#include <hms/Core/Format.h>
#include <hms/Core/Tui.h>
#include <hms/Core/Validation.h>
#include <hms/Hospital.h>
#include <hms/Models/Medicine.h>
#include <hms/Modules/Pharmacy.h>

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iostream>
#include <queue>
#include <vector>

namespace hms {


void PharmacyModule::logAudit(const std::string& dataDir, const std::string& action, const std::string& details) {
    std::ofstream log(dataDir + "/audit.log", std::ios::app);
    if (log.is_open()) {
        std::time_t now = std::time(nullptr);
        log << Validator::formatDateTime(now) << " | "
            << action << " | " << details << "\n";
    }
}

std::vector<Medicine> PharmacyModule::filterMedicines(const std::vector<Medicine>& all, const std::string& query) {
    std::vector<Medicine> result;
    if (query.empty()) return all;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
        [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
    for (const auto& m : all) {
        std::string lowerName = m.name();
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
            [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
        std::string lowerSku = m.sku();
        std::transform(lowerSku.begin(), lowerSku.end(), lowerSku.begin(),
            [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
        if (lowerName.find(lowerQuery) != std::string::npos ||
            lowerSku.find(lowerQuery) != std::string::npos) {
            result.push_back(m);
        }
    }
    return result;
}

bool PharmacyModule::ExpiringFirst::operator()(
    const Medicine* a, const Medicine* b) const {
    return a->expiry() > b->expiry();
}

std::vector<Medicine*> PharmacyModule::sortedByExpiry(std::vector<Medicine>& medicines) {
    std::priority_queue<Medicine*, std::vector<Medicine*>, ExpiringFirst> heap;
    for (auto& medicine : medicines) heap.push(&medicine);
    std::vector<Medicine*> ordered;
    while (!heap.empty()) {
        ordered.push_back(heap.top());
        heap.pop();
    }
    return ordered;
}

void PharmacyModule::listMedicines() {
    std::string query;
    while (true) {
        tui_.clearScreen();
        auto filtered = filterMedicines(hospital_.medicines(), query);
        if (filtered.empty()) {
            tui_.toast("No medicines found.", Tui::Level::Info);
            tui_.pause();
            query.clear();
            continue;
        }
        std::string title = query.empty() ? "ALL MEDICINES"
            : "MEDICINES MATCHING \"" + query + "\"";

        std::vector<std::string> headers{ "SKU", "Name", "Stock", "Reorder@", "Price", "Expires" };
        std::vector<std::vector<std::string>> rows;
        std::time_t day = Validator::today();
        for (const auto& medicine : filtered) {
            std::string stockCell = std::to_string(medicine.stock());
            if (medicine.isLowStock())
                stockCell = std::string(Tui::Color::WHITE) + stockCell + " (low)" + Tui::Color::RESET;
            std::string expiryCell = Validator::formatDate(medicine.expiry());
            int days = medicine.daysUntilExpiry(day);
            if (days < 0)
                expiryCell = std::string(Tui::Color::WHITE) + expiryCell + " EXPIRED" + Tui::Color::RESET;
            else if (days <= 30)
                expiryCell = std::string(Tui::Color::WHITE) + expiryCell +
                             " (" + std::to_string(days) + "d)" + Tui::Color::RESET;
            rows.push_back({
                medicine.sku(), medicine.name(), stockCell,
                std::to_string(medicine.reorderLevel()), Formatter::money(medicine.unitPrice()), expiryCell,
            });
        }
        int bw = tui_.bannerOpen(title, std::to_string(filtered.size()) + " found",
                                 {"Home", "Pharmacy", "List"},
                                 tui_.tableBoxWidth(headers, rows));
        tui_.tableInBox(bw, headers, rows);
        std::cout << "\n";

        std::string prompt = "[Enter] Back";
        if (!query.empty()) prompt += "  [C] Clear";
        prompt += "  [type to filter]";
        std::string input = validation_.readLine(prompt, true);
        if (input.empty()) return;
        if ((input == "c" || input == "C") && !query.empty()) {
            query.clear();
            continue;
        }
        query = input;
    }
}

void PharmacyModule::editMedicine() {
    if (hospital_.medicines().empty()) {
        tui_.toast("No medicines to edit.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    tui_.clearScreen();

    std::vector<std::string> headers{"#", "SKU", "Name", "Stock", "Price", "Expires"};
    std::vector<std::vector<std::string>> rows;
    std::time_t today = Validator::today();
    int rowNum = 1;
    for (const auto& medicine : hospital_.medicines()) {
        int days = medicine.daysUntilExpiry(today);
        std::string expiryCell = Validator::formatDate(medicine.expiry());
        if (days < 0) expiryCell += " EXPIRED";
        else if (days <= 30) expiryCell += " (" + std::to_string(days) + "d)";
        rows.push_back({
            std::to_string(rowNum++),
            medicine.sku(), medicine.name(),
            std::to_string(medicine.stock()),
            Formatter::money(medicine.unitPrice()), expiryCell,
        });
    }

    int bw = tui_.bannerOpen("SELECT MEDICINE TO EDIT", "",
                             {"Home", "Pharmacy", "Edit"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";

    int choice = validation_.readInt("Medicine number", 1, static_cast<int>(hospital_.medicines().size()));
    Medicine* medicine = &hospital_.medicines().at(choice - 1);

    std::cout << "\n  Editing: " << medicine->name() << " (" << medicine->sku() << ")\n\n  "
              << Tui::Color::DIM
              << "Leave a value blank to keep it unchanged."
              << Tui::Color::RESET << "\n";

    std::string newName = validation_.readLine("Name [" + medicine->name() + "]", true);
    if (!newName.empty() && Validator::isBlank(newName)) {
        tui_.toast("Name cannot be blank.", Tui::Level::Warning);
        tui_.pause();
        return;
    }

    std::string reorderStr = validation_.readLine(
        "Reorder threshold [" + std::to_string(medicine->reorderLevel()) + "]", true);
    int newReorder = medicine->reorderLevel();
    if (!reorderStr.empty()) {
        try {
            std::size_t pos = 0;
            int v = std::stoi(reorderStr, &pos);
            if (pos != reorderStr.size() || v < 1 || v > 1000000) {
                tui_.toast("Threshold must be between 1 and 1,000,000.", Tui::Level::Warning);
                tui_.pause();
                return;
            }
            newReorder = v;
        } catch (const std::exception&) {
            tui_.toast("Not a valid number.", Tui::Level::Warning);
            tui_.pause();
            return;
        }
    }

    std::string priceStr = validation_.readLine(
        "Unit price [" + Formatter::money(medicine->unitPrice()) + "]", true);
    double newPrice = medicine->unitPrice();
    if (!priceStr.empty()) {
        try {
            std::size_t pos = 0;
            double v = std::stod(priceStr, &pos);
            if (pos != priceStr.size() || v < 0.0 || v > 1000000.0) {
                tui_.toast("Price must be between 0 and 1,000,000.", Tui::Level::Warning);
                tui_.pause();
                return;
            }
            newPrice = v;
        } catch (const std::exception&) {
            tui_.toast("Not a valid number.", Tui::Level::Warning);
            tui_.pause();
            return;
        }
    }

    std::string expiryStr = validation_.readLine(
        "Expiry date (YYYY-MM-DD) [" + Validator::formatDate(medicine->expiry()) + "]", true);
    std::time_t newExpiry = medicine->expiry();
    if (!expiryStr.empty()) {
        try {
            newExpiry = Validator::parseDate(expiryStr);
        } catch (const std::exception&) {
            tui_.toast("Invalid date format. Use YYYY-MM-DD.", Tui::Level::Warning);
            tui_.pause();
            return;
        }
        if (newExpiry <= Validator::today()) {
            tui_.toast("Expiry date must be in the future.", Tui::Level::Warning);
            tui_.pause();
            return;
        }
    }

    tui_.clearScreen();
    std::vector<std::string> cHeaders{"Field", "Old Value", "New Value"};
    std::vector<std::vector<std::string>> cRows;
    bool hasChanges = false;

    if (!newName.empty()) {
        cRows.push_back({"Name", medicine->name(), newName});
        hasChanges = true;
    }
    if (newReorder != medicine->reorderLevel()) {
        cRows.push_back({"Reorder threshold", std::to_string(medicine->reorderLevel()), std::to_string(newReorder)});
        hasChanges = true;
    }
    if (newPrice != medicine->unitPrice()) {
        cRows.push_back({"Unit price", Formatter::money(medicine->unitPrice()), Formatter::money(newPrice)});
        hasChanges = true;
    }
    if (newExpiry != medicine->expiry()) {
        cRows.push_back({"Expiry date", Validator::formatDate(medicine->expiry()), Validator::formatDate(newExpiry)});
        hasChanges = true;
    }

    if (!hasChanges) {
        tui_.toast("No changes made.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    int cw = tui_.bannerOpen("CONFIRM CHANGES",
                             "review before saving",
                             {"Home", "Pharmacy", "Edit", "Confirm"},
                             tui_.tableBoxWidth(cHeaders, cRows));
    tui_.tableInBox(cw, cHeaders, cRows);
    std::cout << "\n";

    if (!tui_.confirm("Save changes?")) {
        tui_.toast("No changes saved.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    if (!newName.empty()) medicine->setName(newName);
    if (newReorder != medicine->reorderLevel()) medicine->setReorderLevel(newReorder);
    if (newPrice != medicine->unitPrice()) medicine->setUnitPrice(newPrice);
    if (newExpiry != medicine->expiry()) medicine->setExpiry(newExpiry);

    hospital_.saveAll();
    logAudit(hospital_.dataDir(), "MEDICINE_UPDATED",
             "sku=" + medicine->sku() + " name=" + medicine->name());
    tui_.toast("Updated.", Tui::Level::Success);
    tui_.pause();
}

void PharmacyModule::deleteMedicine() {
    if (hospital_.medicines().empty()) {
        tui_.toast("No medicines to delete.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    tui_.clearScreen();

    std::vector<std::string> headers{"#", "SKU", "Name", "Stock", "Price", "Expires"};
    std::vector<std::vector<std::string>> rows;
    std::time_t today = Validator::today();
    int rowNum = 1;
    for (const auto& medicine : hospital_.medicines()) {
        int days = medicine.daysUntilExpiry(today);
        std::string expiryCell = Validator::formatDate(medicine.expiry());
        if (days < 0) expiryCell += " EXPIRED";
        else if (days <= 30) expiryCell += " (" + std::to_string(days) + "d)";
        rows.push_back({
            std::to_string(rowNum++),
            medicine.sku(), medicine.name(),
            std::to_string(medicine.stock()),
            Formatter::money(medicine.unitPrice()), expiryCell,
        });
    }

    int bw = tui_.bannerOpen("SELECT MEDICINE TO DELETE", "",
                             {"Home", "Pharmacy", "Delete"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";

    int choice = validation_.readInt("Medicine number", 1, static_cast<int>(hospital_.medicines().size()));
    Medicine* medicine = &hospital_.medicines().at(choice - 1);

    std::cout << "\n  About to delete: " << medicine->name()
              << " (" << medicine->sku() << ")\n\n";

    int billReferences = 0;
    for (const auto& bill : hospital_.bills()) {
        for (const auto& item : bill.items()) {
            if (item.category == ChargeCategory::Medicine &&
                item.description.find(medicine->name()) != std::string::npos) {
                ++billReferences;
                break;
            }
        }
    }
    if (billReferences > 0) {
        std::cout << "  " << Tui::Color::YELLOW
                  << "[!] This medicine appears in " << billReferences
                  << " bill(s)." << Tui::Color::RESET << "\n\n";
        if (!tui_.confirm("Proceed with deletion? Bill references will remain.")) {
            tui_.toast("Deletion cancelled.", Tui::Level::Info);
            tui_.pause();
            return;
        }
    }

    if (!tui_.confirm("Confirm deletion? This cannot be undone.")) {
        tui_.toast("Deletion cancelled.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    std::string deletedSku = medicine->sku();
    std::string deletedName = medicine->name();
    auto& medicines = hospital_.medicines();
    medicines.erase(std::remove_if(medicines.begin(), medicines.end(),
        [deletedSku](const Medicine& m) { return m.sku() == deletedSku; }), medicines.end());
    hospital_.saveAll();
    logAudit(hospital_.dataDir(), "MEDICINE_DELETED",
             "sku=" + deletedSku + " name=" + deletedName);
    tui_.toast("Deleted " + deletedName + ".", Tui::Level::Success);
    tui_.pause();
}

void PharmacyModule::addMedicine() {
    tui_.clearScreen();
    tui_.banner("ADD NEW MEDICINE", "enter details for a new medicine", {"Home", "Pharmacy", "Add"});
    std::cout << "\n";

    std::vector<std::string> aHeaders{"SKU", "Name", "Stock", "Price", "Expires"};
    std::vector<std::vector<std::string>> aRows;
    std::time_t aDay = Validator::today();
    for (const auto& medicine : hospital_.medicines()) {
        int days = medicine.daysUntilExpiry(aDay);
        std::string expiryCell = Validator::formatDate(medicine.expiry());
        if (days < 0) expiryCell += " EXPIRED";
        else if (days <= 30) expiryCell += " (" + std::to_string(days) + "d)";
        aRows.push_back({
            medicine.sku(), medicine.name(),
            std::to_string(medicine.stock()),
            Formatter::money(medicine.unitPrice()), expiryCell,
        });
    }
    if (!aRows.empty()) {
        int aw = tui_.bannerOpen("CURRENT INVENTORY", "avoid duplicate SKUs",
                                 {"Home", "Pharmacy", "Add"},
                                 tui_.tableBoxWidth(aHeaders, aRows));
        tui_.tableInBox(aw, aHeaders, aRows);
        std::cout << "\n";
    }

    std::string sku = validation_.readNonEmpty("SKU code");
    std::transform(sku.begin(), sku.end(), sku.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    Medicine* existing = hospital_.findMedicine(sku);
    if (existing) {
        tui_.toast("SKU '" + sku + "' already exists (" + existing->name() + "). Use 'Restock medicine' to add quantity.",
                   Tui::Level::Warning);
        tui_.pause();
        return;
    }

    std::string name = validation_.readNonEmpty("Name");
    int stock = validation_.readInt("Initial stock", 0, 1000000);
    int reorder = validation_.readInt("Reorder threshold", 1, 1000000);
    double price = validation_.readDouble("Unit price", 0.0, 1000000.0);
    std::time_t expiry = validation_.readDate("Expiry date");

    std::time_t today = Validator::today();
    if (expiry <= today) {
        tui_.toast("Expiry date must be in the future.", Tui::Level::Warning);
        tui_.pause();
        return;
    }

    hospital_.medicines().push_back(
        Medicine(sku, name, stock, reorder, price, expiry));
    hospital_.saveAll();
    logAudit(hospital_.dataDir(), "MEDICINE_ADDED",
             "sku=" + sku + " name=" + name + " stock=" + std::to_string(stock));
    tui_.toast("Added " + name + " (" + sku + ").", Tui::Level::Success);
    tui_.pause();
}

void PharmacyModule::restockMedicine() {
    if (hospital_.medicines().empty()) {
        tui_.toast("No medicines in inventory. Use 'Add new medicine' first.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    tui_.clearScreen();

    std::vector<std::string> headers{"#", "SKU", "Name", "Stock", "Price", "Expires"};
    std::vector<std::vector<std::string>> rows;
    std::time_t today = Validator::today();
    int rowNum = 1;
    for (const auto& medicine : hospital_.medicines()) {
        int days = medicine.daysUntilExpiry(today);
        std::string expiryCell = Validator::formatDate(medicine.expiry());
        if (days < 0) expiryCell += " EXPIRED";
        else if (days <= 30) expiryCell += " (" + std::to_string(days) + "d)";
        rows.push_back({
            std::to_string(rowNum++),
            medicine.sku(), medicine.name(),
            std::to_string(medicine.stock()),
            Formatter::money(medicine.unitPrice()), expiryCell,
        });
    }

    int bw = tui_.bannerOpen("SELECT MEDICINE TO RESTOCK", "",
                             {"Home", "Pharmacy", "Restock"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";

    int choice = validation_.readInt("Medicine number", 1, static_cast<int>(hospital_.medicines().size()));
    Medicine* medicine = &hospital_.medicines().at(choice - 1);

    int qty = validation_.readInt("Quantity to add", 1, 1000000);
    medicine->restock(qty);
    hospital_.saveAll();
    tui_.toast(medicine->name() + " restocked to " +
               std::to_string(medicine->stock()) + " units.",
               Tui::Level::Success);
    tui_.pause();
}

void PharmacyModule::dispense() {
    if (hospital_.medicines().empty()) {
        tui_.toast("No medicines in inventory.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    tui_.clearScreen();

    std::vector<std::string> headers{"#", "SKU", "Name", "Stock", "Price", "Expires"};
    std::vector<std::vector<std::string>> rows;
    std::time_t today = Validator::today();
    int rowNum = 1;
    for (const auto& medicine : hospital_.medicines()) {
        int days = medicine.daysUntilExpiry(today);
        std::string stockCell = std::to_string(medicine.stock());
        if (medicine.stock() <= 0) stockCell = std::string(Tui::Color::WHITE) + stockCell + " (out)" + Tui::Color::RESET;
        else if (medicine.isLowStock()) stockCell = std::string(Tui::Color::WHITE) + stockCell + " (low)" + Tui::Color::RESET;
        std::string expiryCell = Validator::formatDate(medicine.expiry());
        if (days < 0) expiryCell = std::string(Tui::Color::WHITE) + expiryCell + " EXPIRED" + Tui::Color::RESET;
        else if (days <= 30) expiryCell += " (" + std::to_string(days) + "d)";
        rows.push_back({
            std::to_string(rowNum++),
            medicine.sku(), medicine.name(),
            stockCell,
            Formatter::money(medicine.unitPrice()), expiryCell,
        });
    }

    int bw = tui_.bannerOpen("SELECT MEDICINE TO DISPENSE", "",
                             {"Home", "Pharmacy", "Dispense"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";

    int choice = validation_.readInt("Medicine number", 1, static_cast<int>(hospital_.medicines().size()));
    Medicine* medicine = &hospital_.medicines().at(choice - 1);

    if (medicine->isExpired(today)) {
        throw InvalidInputException(
            "this batch is expired (" +
            Validator::formatDate(medicine->expiry()) + ")");
    }
    if (medicine->stock() <= 0) {
        throw CapacityException("stock is zero");
    }
    int daysLeft = medicine->daysUntilExpiry(today);
    if (daysLeft <= 30) {
        std::cout << "\n  " << Tui::Color::YELLOW
                  << "[!] This medicine expires in " << daysLeft << " day(s) ("
                  << Validator::formatDate(medicine->expiry()) << ")."
                  << Tui::Color::RESET << "\n\n";
        if (!tui_.confirm("Dispense anyway?")) {
            tui_.toast("Dispensing cancelled.", Tui::Level::Info);
            tui_.pause();
            return;
        }
    }
    int qty = validation_.readInt("Quantity to dispense",
                                  1, medicine->stock());
    medicine->dispense(qty);
    hospital_.saveAll();
    double cost = qty * medicine->unitPrice();
    tui_.toast("Dispensed " + std::to_string(qty) + " x " +
               medicine->name() + " — " + Formatter::money(cost),
               Tui::Level::Success);
    tui_.pause();
}

void PharmacyModule::lowStockAlerts() {
    tui_.clearScreen();

    std::vector<std::string> headers{ "SKU", "Name", "Stock", "Threshold", "Suggested" };
    std::vector<std::vector<std::string>> rows;
    for (const auto& medicine : hospital_.medicines()) {
        if (!medicine.isLowStock()) continue;
        rows.push_back({
            medicine.sku(), medicine.name(),
            std::to_string(medicine.stock()),
            std::to_string(medicine.reorderLevel()),
            std::to_string(medicine.reorderLevel() - medicine.stock() + 1) + " to reorder",
        });
    }
    int bw = tui_.bannerOpen("LOW STOCK ALERTS", "items at or below reorder threshold",
                             {"Home", "Pharmacy", "Low Stock"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";
    tui_.pause();
}

void PharmacyModule::nearExpiryAlerts() {
    tui_.clearScreen();

    std::time_t today = Validator::today();
    constexpr int windowDays = 30;
    auto ordered = sortedByExpiry(hospital_.medicines());

    std::vector<std::string> headers{ "SKU", "Name", "Stock", "Expires", "Days left" };
    std::vector<std::vector<std::string>> rows;
    for (const Medicine* medicine : ordered) {
        int days = medicine->daysUntilExpiry(today);
        if (days > windowDays) break;
        rows.push_back({
            medicine->sku(), medicine->name(),
            std::to_string(medicine->stock()),
            Validator::formatDate(medicine->expiry()),
            std::string(Tui::Color::WHITE) + std::to_string(days) + "d" + Tui::Color::RESET,
        });
    }
    int bw = tui_.bannerOpen("NEAR EXPIRY ALERTS",
                             "expiring within " + std::to_string(windowDays) + " days",
                             {"Home", "Pharmacy", "Near Expiry"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";
    tui_.pause();
}


void PharmacyModule::run() {
    while (true) {
        char choice = tui_.menu(
            "PHARMACY",
            {"Home", "Pharmacy"},
            {
                { '1', "Add new medicine",     "add to inventory" },
                { '2', "Edit medicine",        "update an existing record" },
                { '3', "Delete medicine",      "remove from inventory" },
                { '4', "List all medicines",   "search and view" },
                { '5', "Restock medicine",     "add quantity to existing" },
                { '6', "Dispense",             "decrement stock" },
                { '7', "Low stock alerts",     "below reorder threshold" },
                { '8', "Near-expiry alerts",   "soonest first" },
                { 'B', "Back",                 "return to main menu" },
            });
        switch (choice) {
            case '1': addMedicine();       break;
            case '2': editMedicine();      break;
            case '3': deleteMedicine();    break;
            case '4': listMedicines();     break;
            case '5': restockMedicine();   break;
            case '6': dispense();          break;
            case '7': lowStockAlerts();    break;
            case '8': nearExpiryAlerts();  break;
            case 'B': return;
        }
    }
}

}
