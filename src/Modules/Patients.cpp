#include <hms/Core/Exceptions.h>
#include <hms/Core/Tui.h>
#include <hms/Core/Validation.h>
#include <hms/Hospital.h>
#include <hms/Models/Patient.h>
#include <hms/Modules/Patients.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iostream>
#include <vector>

namespace hms {


void PatientsModule::logAudit(const std::string& dataDir, const std::string& action, const std::string& details) {
    std::ofstream log(dataDir + "/audit.log", std::ios::app);
    if (log.is_open()) {
        std::time_t now = std::time(nullptr);
        log << Validator::formatDateTime(now) << " | "
            << action << " | " << details << "\n";
    }
}

void PatientsModule::showPatientList(const std::vector<Patient>& patients, const std::string& title) {
    std::vector<std::string> headers{ "ID", "Name", "Age", "Sex", "Severity", "Flags", "Status" };
    std::vector<std::vector<std::string>> rows;
    for (const auto& patient : patients) {
        std::vector<std::string> flagParts;
        if (patient.isSenior())      flagParts.push_back("Senior");
        if (patient.isPWD())         flagParts.push_back("PWD");
        if (patient.hasPhilHealth()) flagParts.push_back("PhilHealth");
        std::string flags = flagParts.empty() ? "None" : flagParts[0];
        for (std::size_t i = 1; i < flagParts.size(); ++i) {
            flags += ", " + flagParts[i];
        }
        std::string status = patient.isAdmitted() ? "admitted"
                           : patient.hasDoctor()  ? "with doctor"
                                                  : "waiting";
        std::string sexLabel = (patient.sex() == 'M' || patient.sex() == 'm') ? "Male" : "Female";
        rows.push_back({
            std::to_string(patient.id()),
            patient.name(),
            std::to_string(patient.age()),
            sexLabel,
            std::string(severityColor(patient.severity())) +
                severityLabel(patient.severity()) + Tui::Color::RESET,
            flags, status,
        });
    }
    int bw = tui_.bannerOpen(title, "", {"Home", "Patients", "List"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";
}

std::vector<Patient> PatientsModule::filterPatients(const std::vector<Patient>& all, const std::string& query) {
    std::vector<Patient> result;
    if (query.empty()) return all;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
        [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
    for (const auto& p : all) {
        std::string lowerName = p.name();
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
            [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
        if (lowerName.find(lowerQuery) != std::string::npos) result.push_back(p);
    }
    std::sort(result.begin(), result.end(), [](const Patient& a, const Patient& b) {
        return a.name() < b.name() || (a.name() == b.name() && a.id() < b.id());
    });
    return result;
}

void PatientsModule::addPatient() {
    tui_.clearScreen();
    tui_.banner("REGISTER NEW PATIENT", "", {"Home", "Patients", "Add"});
    std::cout << "\n";

    std::string name    = validation_.readName("Full name");
    int age             = validation_.readInt("Age", 0, 130);
    char sex            = validation_.readChar("Sex (M/F)", "MF");
    std::string contact = validation_.readContact("Contact number");
    std::string address = validation_.readAddress("Address");
    int severity        = validation_.readInt(
        "Initial severity (1-5, 1 if walk-in)", 1, 5);
    char senior     = (age >= 60) ? 'Y' : 'N';
    std::cout << "  Senior citizen: " << (senior == 'Y' ? "Yes" : "No")
              << " (auto-detected from age)\n";
    char pwd        = validation_.readChar("PWD? (Y/N)",              "YN");
    char philhealth = validation_.readChar("Has PhilHealth? (Y/N)",   "YN");
    std::string promoCode = validation_.readLine("Promo code (optional)", true);
    if (!Validator::isBlank(promoCode)) {
        /* keep as-is */
    } else if (!promoCode.empty()) {
        tui_.toast("Promo code is whitespace-only; treating as empty.", Tui::Level::Warning);
        promoCode.clear();
    }

    for (const auto& existing : hospital_.patients()) {
        if (existing.contact() == contact) {
            tui_.toast("A patient with this contact number already exists: "
                       + existing.name() + " (id #" + std::to_string(existing.id()) + ").",
                       Tui::Level::Error);
            tui_.pause();
            return;
        }
    }

    for (const auto& existing : hospital_.patients()) {
        std::string eName = existing.name();
        std::transform(eName.begin(), eName.end(), eName.begin(),
            [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
        std::string nName = name;
        std::transform(nName.begin(), nName.end(), nName.begin(),
            [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
        if (eName == nName && existing.age() == age) {
            std::cout << "\n  " << Tui::Color::YELLOW
                      << "[!] A patient with this name and age already exists:"
                      << Tui::Color::RESET << "\n"
                      << "      #" << existing.id() << "  "
                      << existing.name() << " (" << existing.age()
                      << existing.sex() << ")\n\n";
            if (tui_.confirm("Cancel registration?")) {
                tui_.toast("Registration cancelled.", Tui::Level::Info);
                tui_.pause();
                return;
            }
            break;
        }
    }

    tui_.clearScreen();
    std::vector<std::string> headers{"Field", "Value"};
    std::vector<std::vector<std::string>> rows{
        {"Name", name},
        {"Age / Sex", std::to_string(age) + " / " + std::string(1, sex)},
        {"Contact", contact},
        {"Address", address},
        {"Severity", severityLabel(severity)},
        {"Senior citizen", senior == 'Y' ? "Yes" : "No"},
        {"PWD", pwd == 'Y' ? "Yes" : "No"},
        {"PhilHealth", philhealth == 'Y' ? "Yes" : "No"},
        {"Promo code", promoCode.empty() ? "(none)" : promoCode},
    };
    int bw = tui_.bannerOpen("CONFIRM PATIENT DETAILS",
                             "review before registering",
                             {"Home", "Patients", "Add", "Confirm"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";

    if (!tui_.confirm("Confirm registration?")) {
        tui_.toast("Registration cancelled.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    Patient patient(hospital_.nextPatientId(), name, age, sex, contact, address);
    patient.setSeverity(severity);
    patient.setArrival(Validator::now());
    patient.setSenior(senior == 'Y');
    patient.setPWD(pwd == 'Y');
    patient.setPhilHealth(philhealth == 'Y');
    patient.setPromoCode(promoCode);

    hospital_.patients().push_back(patient);
    hospital_.saveAll();
    logAudit(hospital_.dataDir(), "PATIENT_REGISTERED",
             "id=" + std::to_string(patient.id()) + " name=" + name +
             " age=" + std::to_string(age) + " sex=" + std::string(1, sex));
    tui_.toast("Registered " + name + " (id #" +
               std::to_string(patient.id()) + ").",
               Tui::Level::Success);
    tui_.pause();
}

void PatientsModule::editPatient() {
    tui_.clearScreen();
    tui_.banner("EDIT PATIENT", "", {"Home", "Patients", "Edit"});
    std::cout << "\n";

    std::vector<std::string> pHeaders{"ID", "Name", "Age", "Sex", "Severity", "Flags", "Status"};
    std::vector<std::vector<std::string>> pRows;
    for (const auto& p : hospital_.patients()) {
        std::vector<std::string> fp;
        if (p.isSenior()) fp.push_back("Senior");
        if (p.isPWD()) fp.push_back("PWD");
        if (p.hasPhilHealth()) fp.push_back("PhilHealth");
        std::string fl = fp.empty() ? "None" : fp[0];
        for (std::size_t i = 1; i < fp.size(); ++i) fl += ", " + fp[i];
        std::string st = p.isAdmitted() ? "admitted" : p.hasDoctor() ? "with doctor" : "waiting";
        std::string sx = (p.sex() == 'M' || p.sex() == 'm') ? "Male" : "Female";
        pRows.push_back({
            std::to_string(p.id()), p.name(), std::to_string(p.age()), sx,
            std::string(severityColor(p.severity())) + severityLabel(p.severity()) + Tui::Color::RESET,
            fl, st,
        });
    }
    int pw = tui_.bannerOpen("SELECT PATIENT TO EDIT", "",
                             {"Home", "Patients", "Edit"},
                             tui_.tableBoxWidth(pHeaders, pRows));
    tui_.tableInBox(pw, pHeaders, pRows);
    std::cout << "\n";
    int id = validation_.readInt("Patient id", 1, 1000000);
    Patient* patient = hospital_.findPatient(id);
    if (!patient) throw NotFoundException("patient #" + std::to_string(id));

    std::cout << "\n  Editing: " << patient->summary() << "\n\n  "
              << Tui::Color::DIM
              << "Leave a value blank to keep it unchanged."
              << Tui::Color::RESET << "\n";

    std::string newName = validation_.readLine("Name [" + patient->name() + "]", true);
    if (!newName.empty() && newName.size() < 2) {
        tui_.toast("Name must be at least 2 characters.", Tui::Level::Warning);
        tui_.pause();
        return;
    }

    std::string newContact = validation_.readLine(
        "Contact [" + patient->contact() + "]", true);
    if (!newContact.empty()) {
        bool allDigits = true;
        for (char c : newContact) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                allDigits = false;
                break;
            }
        }
        if (!allDigits || newContact.size() != 11 ||
            newContact[0] != '0' || newContact[1] != '9') {
            tui_.toast("Contact must be 11 digits starting with 09.", Tui::Level::Warning);
            tui_.pause();
            return;
        }
        for (const auto& existing : hospital_.patients()) {
            if (existing.id() == patient->id()) continue;
            if (existing.contact() == newContact) {
                tui_.toast("Another patient already uses this contact number.", Tui::Level::Warning);
                tui_.pause();
                return;
            }
        }
    }

    std::string newAddress = validation_.readLine(
        "Address [" + patient->address() + "]", true);
    if (!newAddress.empty() && newAddress.size() < 10) {
        tui_.toast("Address must be at least 10 characters.", Tui::Level::Warning);
        tui_.pause();
        return;
    }

    std::string ageStr = validation_.readLine(
        "Age [" + std::to_string(patient->age()) + "]", true);
    int newAge = patient->age();
    if (!ageStr.empty()) {
        try {
            std::size_t pos = 0;
            int v = std::stoi(ageStr, &pos);
            if (pos != ageStr.size() || v < 0 || v > 130) {
                tui_.toast("Age must be between 0 and 130.", Tui::Level::Warning);
                tui_.pause();
                return;
            }
            newAge = v;
        } catch (const std::exception&) {
            tui_.toast("Not a valid number.", Tui::Level::Warning);
            tui_.pause();
            return;
        }
    }

    std::string sevStr = validation_.readLine(
        "Severity 1-5 [" + std::to_string(patient->severity()) + "]", true);
    int newSeverity = patient->severity();
    if (!sevStr.empty()) {
        try {
            std::size_t pos = 0;
            int v = std::stoi(sevStr, &pos);
            if (pos != sevStr.size() || v < 1 || v > 5) {
                tui_.toast("Severity must be between 1 and 5.", Tui::Level::Warning);
                tui_.pause();
                return;
            }
            newSeverity = v;
        } catch (const std::exception&) {
            tui_.toast("Not a valid number.", Tui::Level::Warning);
            tui_.pause();
            return;
        }
    }

    char newSenior = (newAge >= 60) ? 'Y' : 'N';
    if (newAge != patient->age()) {
        std::cout << "  Senior citizen: " << (newSenior == 'Y' ? "Yes" : "No")
                  << " (auto-updated from new age)\n";
    }

    tui_.clearScreen();
    std::vector<std::string> headers{"Field", "Old Value", "New Value"};
    std::vector<std::vector<std::string>> rows;
    bool hasChanges = false;

    if (!newName.empty()) {
        rows.push_back({"Name", patient->name(), newName});
        hasChanges = true;
    }
    if (!newContact.empty()) {
        rows.push_back({"Contact", patient->contact(), newContact});
        hasChanges = true;
    }
    if (!newAddress.empty()) {
        rows.push_back({"Address", patient->address(), newAddress});
        hasChanges = true;
    }
    if (newAge != patient->age()) {
        rows.push_back({"Age", std::to_string(patient->age()), std::to_string(newAge)});
        hasChanges = true;
    }
    if (newSeverity != patient->severity()) {
        rows.push_back({"Severity", severityLabel(patient->severity()), severityLabel(newSeverity)});
        hasChanges = true;
    }
    if (!hasChanges) {
        tui_.toast("No changes made.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    int bw = tui_.bannerOpen("CONFIRM CHANGES",
                             "review before saving",
                             {"Home", "Patients", "Edit", "Confirm"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";

    if (!tui_.confirm("Save changes?")) {
        tui_.toast("No changes saved.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    if (!newName.empty()) patient->setName(newName);
    if (!newContact.empty()) patient->setContact(newContact);
    if (!newAddress.empty()) patient->setAddress(newAddress);
    if (newAge != patient->age()) {
        patient->setAge(newAge);
        patient->setSenior(newSenior == 'Y');
    }
    if (newSeverity != patient->severity()) patient->setSeverity(newSeverity);

    hospital_.saveAll();
    logAudit(hospital_.dataDir(), "PATIENT_UPDATED",
             "id=" + std::to_string(patient->id()) + " name=" + patient->name());
    tui_.toast("Updated.", Tui::Level::Success);
    tui_.pause();
}

void PatientsModule::deletePatient() {
    tui_.clearScreen();
    tui_.banner("DELETE PATIENT", "", {"Home", "Patients", "Delete"});
    std::cout << "\n";

    if (hospital_.patients().empty()) {
        tui_.toast("No patients to delete.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    std::vector<std::string> pHeaders{"ID", "Name", "Age", "Sex", "Severity", "Flags", "Status"};
    std::vector<std::vector<std::string>> pRows;
    for (const auto& p : hospital_.patients()) {
        std::vector<std::string> fp;
        if (p.isSenior()) fp.push_back("Senior");
        if (p.isPWD()) fp.push_back("PWD");
        if (p.hasPhilHealth()) fp.push_back("PhilHealth");
        std::string fl = fp.empty() ? "None" : fp[0];
        for (std::size_t i = 1; i < fp.size(); ++i) fl += ", " + fp[i];
        std::string st = p.isAdmitted() ? "admitted" : p.hasDoctor() ? "with doctor" : "waiting";
        std::string sx = (p.sex() == 'M' || p.sex() == 'm') ? "Male" : "Female";
        pRows.push_back({
            std::to_string(p.id()), p.name(), std::to_string(p.age()), sx,
            std::string(severityColor(p.severity())) + severityLabel(p.severity()) + Tui::Color::RESET,
            fl, st,
        });
    }
    int pw = tui_.bannerOpen("SELECT PATIENT TO DELETE", "",
                             {"Home", "Patients", "Delete"},
                             tui_.tableBoxWidth(pHeaders, pRows));
    tui_.tableInBox(pw, pHeaders, pRows);
    std::cout << "\n";
    int id = validation_.readInt("Patient id", 1, 1000000);
    Patient* patient = hospital_.findPatient(id);
    if (!patient) throw NotFoundException("patient #" + std::to_string(id));

    if (patient->isAdmitted()) {
        tui_.toast("Cannot delete: patient is currently admitted to bed #" +
                   std::to_string(patient->bedId()) + ".", Tui::Level::Error);
        tui_.pause();
        return;
    }
    if (patient->hasDoctor()) {
        tui_.toast("Cannot delete: patient is assigned to doctor #" +
                   std::to_string(patient->doctorId()) + ".", Tui::Level::Error);
        tui_.pause();
        return;
    }

    int activeAppointments = 0;
    for (const auto& appt : hospital_.appointments()) {
        if (appt.patientId() == patient->id() &&
            appt.status() != AppointmentStatus::Cancelled) {
            ++activeAppointments;
        }
    }
    if (activeAppointments > 0) {
        tui_.toast("Cannot delete: patient has " + std::to_string(activeAppointments) +
                   " active appointment(s). Cancel them first.", Tui::Level::Error);
        tui_.pause();
        return;
    }

    int unpaidBills = 0;
    for (const auto& bill : hospital_.bills()) {
        if (bill.patientId() == patient->id() &&
            bill.status() != BillStatus::Voided) {
            ++unpaidBills;
        }
    }
    if (unpaidBills > 0) {
        tui_.toast("Cannot delete: patient has " + std::to_string(unpaidBills) +
                   " bill(s). Void or settle them first.", Tui::Level::Error);
        tui_.pause();
        return;
    }

    std::cout << "\n  About to delete: " << patient->name()
              << " (id #" << patient->id() << ")\n\n";
    if (!tui_.confirm("Confirm deletion? This cannot be undone.")) {
        tui_.toast("Deletion cancelled.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    std::string deletedName = patient->name();
    auto& patients = hospital_.patients();
    patients.erase(std::remove_if(patients.begin(), patients.end(),
        [id](const Patient& p) { return p.id() == id; }), patients.end());
    hospital_.saveAll();
    logAudit(hospital_.dataDir(), "PATIENT_DELETED",
             "id=" + std::to_string(id) + " name=" + deletedName);
    tui_.toast("Deleted " + deletedName + ".", Tui::Level::Success);
    tui_.pause();
}

void PatientsModule::listPatients() {
    std::string query;
    while (true) {
        tui_.clearScreen();
        auto filtered = filterPatients(hospital_.patients(), query);
        if (filtered.empty()) {
            tui_.toast("No patients found.", Tui::Level::Info);
            tui_.pause();
            query.clear();
            continue;
        }
        std::string title = query.empty() ? "ALL PATIENTS"
            : "PATIENTS MATCHING \"" + query + "\"";
        showPatientList(filtered, title + " (" + std::to_string(filtered.size()) + " found)");
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


void PatientsModule::run() {
    while (true) {
        char choice = tui_.menu(
            "PATIENT RECORDS",
            {"Home", "Patients"},
            {
                { '1', "Register patient",   "add a new patient" },
                { '2', "Edit patient",       "update an existing record" },
                { '3', "Delete patient",     "remove a patient record" },
                { '4', "List all patients",  "search and view" },
                { 'B', "Back",               "return to main menu" },
            });
        switch (choice) {
            case '1': addPatient();    break;
            case '2': editPatient();   break;
            case '3': deletePatient(); break;
            case '4': listPatients();  break;
            case 'B': return;
        }
    }
}

}
