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

namespace hms::pharmacy {

namespace {

void logAudit(const std::string& dataDir, const std::string& action, const std::string& details) {
    std::ofstream log(dataDir + "/audit.log", std::ios::app);
    if (log.is_open()) {
        std::time_t now = std::time(nullptr);
        log << validation::formatDateTime(now) << " | "
            << action << " | " << details << "\n";
    }
}

std::vector<Medicine> filterMedicines(const std::vector<Medicine>& all, const std::string& query) {
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

struct ExpiringFirst {
    bool operator()(const Medicine* a, const Medicine* b) const {
        return a->expiry() > b->expiry();
    }
};

std::vector<Medicine*> sortedByExpiry(std::vector<Medicine>& medicines) {
    std::priority_queue<Medicine*, std::vector<Medicine*>, ExpiringFirst> heap;
    for (auto& medicine : medicines) heap.push(&medicine);
    std::vector<Medicine*> ordered;
    while (!heap.empty()) {
        ordered.push_back(heap.top());
        heap.pop();
    }
    return ordered;
}

void listMedicines(Hospital& hospital) {
    std::string query;
    while (true) {
        tui::clearScreen();
        auto filtered = filterMedicines(hospital.medicines(), query);
        if (filtered.empty()) {
            tui::toast("No medicines found.", tui::Level::Info);
            tui::pause();
            query.clear();
            continue;
        }
        std::string title = query.empty() ? "ALL MEDICINES"
            : "MEDICINES MATCHING \"" + query + "\"";

        std::vector<std::string> headers{ "SKU", "Name", "Stock", "Reorder@", "Price", "Expires" };
        std::vector<std::vector<std::string>> rows;
        std::time_t day = validation::today();
        for (const auto& medicine : filtered) {
            std::string stockCell = std::to_string(medicine.stock());
            if (medicine.isLowStock())
                stockCell = std::string(tui::color::WHITE) + stockCell + " (low)" + tui::color::RESET;
            std::string expiryCell = validation::formatDate(medicine.expiry());
            int days = medicine.daysUntilExpiry(day);
            if (days < 0)
                expiryCell = std::string(tui::color::WHITE) + expiryCell + " EXPIRED" + tui::color::RESET;
            else if (days <= 30)
                expiryCell = std::string(tui::color::WHITE) + expiryCell +
                             " (" + std::to_string(days) + "d)" + tui::color::RESET;
            rows.push_back({
                medicine.sku(), medicine.name(), stockCell,
                std::to_string(medicine.reorderLevel()), format::money(medicine.unitPrice()), expiryCell,
            });
        }
        int bw = tui::bannerOpen(title, std::to_string(filtered.size()) + " found",
                                 {"Home", "Pharmacy", "List"},
                                 tui::tableBoxWidth(headers, rows));
        tui::tableInBox(bw, headers, rows);
        std::cout << "\n";

        std::string prompt = "[Enter] Back";
        if (!query.empty()) prompt += "  [C] Clear";
        prompt += "  [type to filter]";
        std::string input = validation::readLine(prompt, true);
        if (input.empty()) return;
        if ((input == "c" || input == "C") && !query.empty()) {
            query.clear();
            continue;
        }
        query = input;
    }
}

void editMedicine(Hospital& hospital) {
    if (hospital.medicines().empty()) {
        tui::toast("No medicines to edit.", tui::Level::Info);
        tui::pause();
        return;
    }

    tui::clearScreen();

    std::vector<std::string> headers{"#", "SKU", "Name", "Stock", "Price", "Expires"};
    std::vector<std::vector<std::string>> rows;
    std::time_t today = validation::today();
    int rowNum = 1;
    for (const auto& medicine : hospital.medicines()) {
        int days = medicine.daysUntilExpiry(today);
        std::string expiryCell = validation::formatDate(medicine.expiry());
        if (days < 0) expiryCell += " EXPIRED";
        else if (days <= 30) expiryCell += " (" + std::to_string(days) + "d)";
        rows.push_back({
            std::to_string(rowNum++),
            medicine.sku(), medicine.name(),
            std::to_string(medicine.stock()),
            format::money(medicine.unitPrice()), expiryCell,
        });
    }

    int bw = tui::bannerOpen("SELECT MEDICINE TO EDIT", "",
                             {"Home", "Pharmacy", "Edit"},
                             tui::tableBoxWidth(headers, rows));
    tui::tableInBox(bw, headers, rows);
    std::cout << "\n";

    int choice = validation::readInt("Medicine number", 1, static_cast<int>(hospital.medicines().size()));
    Medicine* medicine = &hospital.medicines().at(choice - 1);

    std::cout << "\n  Editing: " << medicine->name() << " (" << medicine->sku() << ")\n\n  "
              << tui::color::DIM
              << "Leave a value blank to keep it unchanged."
              << tui::color::RESET << "\n";

    std::string newName = validation::readLine("Name [" + medicine->name() + "]", true);
    if (!newName.empty() && validation::isBlank(newName)) {
        tui::toast("Name cannot be blank.", tui::Level::Warning);
        tui::pause();
        return;
    }

    std::string reorderStr = validation::readLine(
        "Reorder threshold [" + std::to_string(medicine->reorderLevel()) + "]", true);
    int newReorder = medicine->reorderLevel();
    if (!reorderStr.empty()) {
        try {
            std::size_t pos = 0;
            int v = std::stoi(reorderStr, &pos);
            if (pos != reorderStr.size() || v < 1 || v > 1000000) {
                tui::toast("Threshold must be between 1 and 1,000,000.", tui::Level::Warning);
                tui::pause();
                return;
            }
            newReorder = v;
        } catch (const std::exception&) {
            tui::toast("Not a valid number.", tui::Level::Warning);
            tui::pause();
            return;
        }
    }

    std::string priceStr = validation::readLine(
        "Unit price [" + format::money(medicine->unitPrice()) + "]", true);
    double newPrice = medicine->unitPrice();
    if (!priceStr.empty()) {
        try {
            std::size_t pos = 0;
            double v = std::stod(priceStr, &pos);
            if (pos != priceStr.size() || v < 0.0 || v > 1000000.0) {
                tui::toast("Price must be between 0 and 1,000,000.", tui::Level::Warning);
                tui::pause();
                return;
            }
            newPrice = v;
        } catch (const std::exception&) {
            tui::toast("Not a valid number.", tui::Level::Warning);
            tui::pause();
            return;
        }
    }

    std::string expiryStr = validation::readLine(
        "Expiry date (YYYY-MM-DD) [" + validation::formatDate(medicine->expiry()) + "]", true);
    std::time_t newExpiry = medicine->expiry();
    if (!expiryStr.empty()) {
        try {
            newExpiry = validation::parseDate(expiryStr);
        } catch (const std::exception&) {
            tui::toast("Invalid date format. Use YYYY-MM-DD.", tui::Level::Warning);
            tui::pause();
            return;
        }
        if (newExpiry <= validation::today()) {
            tui::toast("Expiry date must be in the future.", tui::Level::Warning);
            tui::pause();
            return;
        }
    }

    tui::clearScreen();
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
        cRows.push_back({"Unit price", format::money(medicine->unitPrice()), format::money(newPrice)});
        hasChanges = true;
    }
    if (newExpiry != medicine->expiry()) {
        cRows.push_back({"Expiry date", validation::formatDate(medicine->expiry()), validation::formatDate(newExpiry)});
        hasChanges = true;
    }

    if (!hasChanges) {
        tui::toast("No changes made.", tui::Level::Info);
        tui::pause();
        return;
    }

    int cw = tui::bannerOpen("CONFIRM CHANGES",
                             "review before saving",
                             {"Home", "Pharmacy", "Edit", "Confirm"},
                             tui::tableBoxWidth(cHeaders, cRows));
    tui::tableInBox(cw, cHeaders, cRows);
    std::cout << "\n";

    if (!tui::confirm("Save changes?")) {
        tui::toast("No changes saved.", tui::Level::Info);
        tui::pause();
        return;
    }

    if (!newName.empty()) medicine->setName(newName);
    if (newReorder != medicine->reorderLevel()) medicine->setReorderLevel(newReorder);
    if (newPrice != medicine->unitPrice()) medicine->setUnitPrice(newPrice);
    if (newExpiry != medicine->expiry()) medicine->setExpiry(newExpiry);

    hospital.saveAll();
    logAudit(hospital.dataDir(), "MEDICINE_UPDATED",
             "sku=" + medicine->sku() + " name=" + medicine->name());
    tui::toast("Updated.", tui::Level::Success);
    tui::pause();
}

void deleteMedicine(Hospital& hospital) {
    if (hospital.medicines().empty()) {
        tui::toast("No medicines to delete.", tui::Level::Info);
        tui::pause();
        return;
    }

    tui::clearScreen();

    std::vector<std::string> headers{"#", "SKU", "Name", "Stock", "Price", "Expires"};
    std::vector<std::vector<std::string>> rows;
    std::time_t today = validation::today();
    int rowNum = 1;
    for (const auto& medicine : hospital.medicines()) {
        int days = medicine.daysUntilExpiry(today);
        std::string expiryCell = validation::formatDate(medicine.expiry());
        if (days < 0) expiryCell += " EXPIRED";
        else if (days <= 30) expiryCell += " (" + std::to_string(days) + "d)";
        rows.push_back({
            std::to_string(rowNum++),
            medicine.sku(), medicine.name(),
            std::to_string(medicine.stock()),
            format::money(medicine.unitPrice()), expiryCell,
        });
    }

    int bw = tui::bannerOpen("SELECT MEDICINE TO DELETE", "",
                             {"Home", "Pharmacy", "Delete"},
                             tui::tableBoxWidth(headers, rows));
    tui::tableInBox(bw, headers, rows);
    std::cout << "\n";

    int choice = validation::readInt("Medicine number", 1, static_cast<int>(hospital.medicines().size()));
    Medicine* medicine = &hospital.medicines().at(choice - 1);

    std::cout << "\n  About to delete: " << medicine->name()
              << " (" << medicine->sku() << ")\n\n";

    int billReferences = 0;
    for (const auto& bill : hospital.bills()) {
        for (const auto& item : bill.items()) {
            if (item.category == ChargeCategory::Medicine &&
                item.description.find(medicine->name()) != std::string::npos) {
                ++billReferences;
                break;
            }
        }
    }
    if (billReferences > 0) {
        std::cout << "  " << tui::color::YELLOW
                  << "[!] This medicine appears in " << billReferences
                  << " bill(s)." << tui::color::RESET << "\n\n";
        if (!tui::confirm("Proceed with deletion? Bill references will remain.")) {
            tui::toast("Deletion cancelled.", tui::Level::Info);
            tui::pause();
            return;
        }
    }

    if (!tui::confirm("Confirm deletion? This cannot be undone.")) {
        tui::toast("Deletion cancelled.", tui::Level::Info);
        tui::pause();
        return;
    }

    std::string deletedSku = medicine->sku();
    std::string deletedName = medicine->name();
    auto& medicines = hospital.medicines();
    medicines.erase(std::remove_if(medicines.begin(), medicines.end(),
        [deletedSku](const Medicine& m) { return m.sku() == deletedSku; }), medicines.end());
    hospital.saveAll();
    logAudit(hospital.dataDir(), "MEDICINE_DELETED",
             "sku=" + deletedSku + " name=" + deletedName);
    tui::toast("Deleted " + deletedName + ".", tui::Level::Success);
    tui::pause();
}

void addMedicine(Hospital& hospital) {
    tui::clearScreen();
    tui::banner("ADD NEW MEDICINE", "enter details for a new medicine", {"Home", "Pharmacy", "Add"});
    std::cout << "\n";

    std::vector<std::string> aHeaders{"SKU", "Name", "Stock", "Price", "Expires"};
    std::vector<std::vector<std::string>> aRows;
    std::time_t aDay = validation::today();
    for (const auto& medicine : hospital.medicines()) {
        int days = medicine.daysUntilExpiry(aDay);
        std::string expiryCell = validation::formatDate(medicine.expiry());
        if (days < 0) expiryCell += " EXPIRED";
        else if (days <= 30) expiryCell += " (" + std::to_string(days) + "d)";
        aRows.push_back({
            medicine.sku(), medicine.name(),
            std::to_string(medicine.stock()),
            format::money(medicine.unitPrice()), expiryCell,
        });
    }
    if (!aRows.empty()) {
        int aw = tui::bannerOpen("CURRENT INVENTORY", "avoid duplicate SKUs",
                                 {"Home", "Pharmacy", "Add"},
                                 tui::tableBoxWidth(aHeaders, aRows));
        tui::tableInBox(aw, aHeaders, aRows);
        std::cout << "\n";
    }

    std::string sku = validation::readNonEmpty("SKU code");
    std::transform(sku.begin(), sku.end(), sku.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    Medicine* existing = hospital.findMedicine(sku);
    if (existing) {
        tui::toast("SKU '" + sku + "' already exists (" + existing->name() + "). Use 'Restock medicine' to add quantity.",
                   tui::Level::Warning);
        tui::pause();
        return;
    }

    std::string name = validation::readNonEmpty("Name");
    int stock = validation::readInt("Initial stock", 0, 1000000);
    int reorder = validation::readInt("Reorder threshold", 1, 1000000);
    double price = validation::readDouble("Unit price", 0.0, 1000000.0);
    std::time_t expiry = validation::readDate("Expiry date");

    std::time_t today = validation::today();
    if (expiry <= today) {
        tui::toast("Expiry date must be in the future.", tui::Level::Warning);
        tui::pause();
        return;
    }

    hospital.medicines().push_back(
        Medicine(sku, name, stock, reorder, price, expiry));
    hospital.saveAll();
    logAudit(hospital.dataDir(), "MEDICINE_ADDED",
             "sku=" + sku + " name=" + name + " stock=" + std::to_string(stock));
    tui::toast("Added " + name + " (" + sku + ").", tui::Level::Success);
    tui::pause();
}

void restockMedicine(Hospital& hospital) {
    if (hospital.medicines().empty()) {
        tui::toast("No medicines in inventory. Use 'Add new medicine' first.", tui::Level::Info);
        tui::pause();
        return;
    }

    tui::clearScreen();

    std::vector<std::string> headers{"#", "SKU", "Name", "Stock", "Price", "Expires"};
    std::vector<std::vector<std::string>> rows;
    std::time_t today = validation::today();
    int rowNum = 1;
    for (const auto& medicine : hospital.medicines()) {
        int days = medicine.daysUntilExpiry(today);
        std::string expiryCell = validation::formatDate(medicine.expiry());
        if (days < 0) expiryCell += " EXPIRED";
        else if (days <= 30) expiryCell += " (" + std::to_string(days) + "d)";
        rows.push_back({
            std::to_string(rowNum++),
            medicine.sku(), medicine.name(),
            std::to_string(medicine.stock()),
            format::money(medicine.unitPrice()), expiryCell,
        });
    }

    int bw = tui::bannerOpen("SELECT MEDICINE TO RESTOCK", "",
                             {"Home", "Pharmacy", "Restock"},
                             tui::tableBoxWidth(headers, rows));
    tui::tableInBox(bw, headers, rows);
    std::cout << "\n";

    int choice = validation::readInt("Medicine number", 1, static_cast<int>(hospital.medicines().size()));
    Medicine* medicine = &hospital.medicines().at(choice - 1);

    int qty = validation::readInt("Quantity to add", 1, 1000000);
    medicine->restock(qty);
    hospital.saveAll();
    tui::toast(medicine->name() + " restocked to " +
               std::to_string(medicine->stock()) + " units.",
               tui::Level::Success);
    tui::pause();
}

void dispense(Hospital& hospital) {
    if (hospital.medicines().empty()) {
        tui::toast("No medicines in inventory.", tui::Level::Info);
        tui::pause();
        return;
    }

    tui::clearScreen();

    std::vector<std::string> headers{"#", "SKU", "Name", "Stock", "Price", "Expires"};
    std::vector<std::vector<std::string>> rows;
    std::time_t today = validation::today();
    int rowNum = 1;
    for (const auto& medicine : hospital.medicines()) {
        int days = medicine.daysUntilExpiry(today);
        std::string stockCell = std::to_string(medicine.stock());
        if (medicine.stock() <= 0) stockCell = std::string(tui::color::WHITE) + stockCell + " (out)" + tui::color::RESET;
        else if (medicine.isLowStock()) stockCell = std::string(tui::color::WHITE) + stockCell + " (low)" + tui::color::RESET;
        std::string expiryCell = validation::formatDate(medicine.expiry());
        if (days < 0) expiryCell = std::string(tui::color::WHITE) + expiryCell + " EXPIRED" + tui::color::RESET;
        else if (days <= 30) expiryCell += " (" + std::to_string(days) + "d)";
        rows.push_back({
            std::to_string(rowNum++),
            medicine.sku(), medicine.name(),
            stockCell,
            format::money(medicine.unitPrice()), expiryCell,
        });
    }

    int bw = tui::bannerOpen("SELECT MEDICINE TO DISPENSE", "",
                             {"Home", "Pharmacy", "Dispense"},
                             tui::tableBoxWidth(headers, rows));
    tui::tableInBox(bw, headers, rows);
    std::cout << "\n";

    int choice = validation::readInt("Medicine number", 1, static_cast<int>(hospital.medicines().size()));
    Medicine* medicine = &hospital.medicines().at(choice - 1);

    if (medicine->isExpired(today)) {
        throw InvalidInputException(
            "this batch is expired (" +
            validation::formatDate(medicine->expiry()) + ")");
    }
    if (medicine->stock() <= 0) {
        throw CapacityException("stock is zero");
    }
    int daysLeft = medicine->daysUntilExpiry(today);
    if (daysLeft <= 30) {
        std::cout << "\n  " << tui::color::YELLOW
                  << "[!] This medicine expires in " << daysLeft << " day(s) ("
                  << validation::formatDate(medicine->expiry()) << ")."
                  << tui::color::RESET << "\n\n";
        if (!tui::confirm("Dispense anyway?")) {
            tui::toast("Dispensing cancelled.", tui::Level::Info);
            tui::pause();
            return;
        }
    }
    int qty = validation::readInt("Quantity to dispense",
                                  1, medicine->stock());
    medicine->dispense(qty);
    hospital.saveAll();
    double cost = qty * medicine->unitPrice();
    tui::toast("Dispensed " + std::to_string(qty) + " x " +
               medicine->name() + " — " + format::money(cost),
               tui::Level::Success);
    tui::pause();
}

void lowStockAlerts(Hospital& hospital) {
    tui::clearScreen();

    std::vector<std::string> headers{ "SKU", "Name", "Stock", "Threshold", "Suggested" };
    std::vector<std::vector<std::string>> rows;
    for (const auto& medicine : hospital.medicines()) {
        if (!medicine.isLowStock()) continue;
        rows.push_back({
            medicine.sku(), medicine.name(),
            std::to_string(medicine.stock()),
            std::to_string(medicine.reorderLevel()),
            std::to_string(medicine.reorderLevel() - medicine.stock() + 1) + " to reorder",
        });
    }
    int bw = tui::bannerOpen("LOW STOCK ALERTS", "items at or below reorder threshold",
                             {"Home", "Pharmacy", "Low Stock"},
                             tui::tableBoxWidth(headers, rows));
    tui::tableInBox(bw, headers, rows);
    std::cout << "\n";
    tui::pause();
}

void nearExpiryAlerts(Hospital& hospital) {
    tui::clearScreen();

    std::time_t today = validation::today();
    constexpr int windowDays = 30;
    auto ordered = sortedByExpiry(hospital.medicines());

    std::vector<std::string> headers{ "SKU", "Name", "Stock", "Expires", "Days left" };
    std::vector<std::vector<std::string>> rows;
    for (const Medicine* medicine : ordered) {
        int days = medicine->daysUntilExpiry(today);
        if (days > windowDays) break;
        rows.push_back({
            medicine->sku(), medicine->name(),
            std::to_string(medicine->stock()),
            validation::formatDate(medicine->expiry()),
            std::string(tui::color::WHITE) + std::to_string(days) + "d" + tui::color::RESET,
        });
    }
    int bw = tui::bannerOpen("NEAR EXPIRY ALERTS",
                             "expiring within " + std::to_string(windowDays) + " days",
                             {"Home", "Pharmacy", "Near Expiry"},
                             tui::tableBoxWidth(headers, rows));
    tui::tableInBox(bw, headers, rows);
    std::cout << "\n";
    tui::pause();
}

}

void run(Hospital& hospital) {
    while (true) {
        char choice = tui::menu(
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
            case '1': addMedicine(hospital);       break;
            case '2': editMedicine(hospital);      break;
            case '3': deleteMedicine(hospital);    break;
            case '4': listMedicines(hospital);     break;
            case '5': restockMedicine(hospital);   break;
            case '6': dispense(hospital);          break;
            case '7': lowStockAlerts(hospital);    break;
            case '8': nearExpiryAlerts(hospital);  break;
            case 'B': return;
        }
    }
}

}
