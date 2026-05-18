#include <hms/Core/Exceptions.h>
#include <hms/Core/Format.h>
#include <hms/Core/Tui.h>
#include <hms/Core/Validation.h>
#include <hms/Hospital.h>
#include <hms/Models/Appointment.h>
#include <hms/Models/Doctor.h>
#include <hms/Models/Patient.h>
#include <hms/Modules/Doctors.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

namespace hms {

void DoctorsModule::logAudit(const std::string& dataDir, const std::string& action, const std::string& details) {
    std::ofstream log(dataDir + "/audit.log", std::ios::app);
    if (log.is_open()) {
        std::time_t now = std::time(nullptr);
        log << Validator::formatDateTime(now) << " | "
            << action << " | " << details << "\n";
    }
}

void DoctorsModule::showDoctorList(const std::vector<Doctor>& doctors, const std::string& title) {
    std::vector<std::string> headers{ "ID", "Name", "Specialty", "Room", "Fee", "Daily limit" };
    std::vector<std::vector<std::string>> rows;
    for (const auto& doctor : doctors) {
        rows.push_back({
            std::to_string(doctor.id()),
            doctor.name(),
            doctor.specialty(),
            doctor.room(),
            Formatter::money(doctor.consultFee()),
            std::to_string(doctor.dailyAppointmentLimit()),
        });
    }
    int bw = tui_.bannerOpen(title, "", {"Home", "Doctors", "List"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";
}

std::vector<Doctor> DoctorsModule::filterDoctors(const std::vector<Doctor>& all, const std::string& query) {
    std::vector<Doctor> result;
    if (query.empty()) return all;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
        [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
    for (const auto& d : all) {
        std::string lowerName = d.name();
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
            [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
        if (lowerName.find(lowerQuery) != std::string::npos) result.push_back(d);
    }
    std::sort(result.begin(), result.end(), [](const Doctor& a, const Doctor& b) {
        return a.name() < b.name() || (a.name() == b.name() && a.id() < b.id());
    });
    return result;
}

void DoctorsModule::addDoctor() {
    auto now = std::chrono::steady_clock::now();
    if (lastDoctorRegistration_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - *lastDoctorRegistration_).count();
        if (elapsed < REGISTRATION_COOLDOWN_SECONDS) {
            tui_.toast("Please wait " +
                       std::to_string(REGISTRATION_COOLDOWN_SECONDS - static_cast<int>(elapsed)) +
                       " seconds before registering another doctor.",
                       Tui::Level::Warning);
            tui_.pause();
            return;
        }
    }

    tui_.clearScreen();
    tui_.banner("REGISTER NEW DOCTOR", "", {"Home", "Doctors", "Add"});
    std::cout << "\n";

    std::string name    = validation_.readName("Full name");
    int age             = validation_.readInt("Age", 28, 90);
    if (age >= 65) {
        std::cout << "  " << Tui::Color::YELLOW
                  << "[!] Age " << age << " is above typical retirement age (65)."
                  << Tui::Color::RESET << "\n\n";
        if (!tui_.confirm("Proceed anyway?")) {
            tui_.toast("Registration cancelled.", Tui::Level::Info);
            tui_.pause();
            return;
        }
    }
    char sex            = validation_.readChar("Sex (M/F)", "MF");
    std::string contact = validation_.readContact("Contact number");
    std::string address = validation_.readAddress("Address");
    std::string specialty = validation_.readSpecialty("Specialty");
    std::string room      = validation_.readRoom("Consulting room");
    double fee            = validation_.readDouble("Consultation fee", 0.0, 1000000.0);
    if (fee > 0.0 && fee < 100.0) {
        std::cout << "  " << Tui::Color::YELLOW
                  << "[!] Consultation fee P" << std::fixed << std::setprecision(2) << fee
                  << " is unusually low." << Tui::Color::RESET << "\n\n";
        if (!tui_.confirm("Proceed anyway?")) {
            tui_.toast("Registration cancelled.", Tui::Level::Info);
            tui_.pause();
            return;
        }
    }
    int dailyLimit        = validation_.readInt(
        "Daily appointment limit", 1, 50);

    for (const auto& existing : hospital_.doctors()) {
        if (existing.contact() == contact) {
            tui_.toast("A doctor with this contact number already exists: "
                       + existing.name() + " (id #" + std::to_string(existing.id()) + ").",
                       Tui::Level::Error);
            tui_.pause();
            return;
        }
    }

    std::string normalizedName = name;
    std::transform(normalizedName.begin(), normalizedName.end(), normalizedName.begin(),
        [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
    for (const auto& existing : hospital_.doctors()) {
        std::string existingName = existing.name();
        std::transform(existingName.begin(), existingName.end(), existingName.begin(),
            [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
        if (existingName == normalizedName) {
            std::cout << "\n  " << Tui::Color::YELLOW
                      << "[!] A doctor with this name already exists:"
                      << Tui::Color::RESET << "\n"
                      << "      #" << existing.id() << "  Dr. "
                      << existing.name() << " (" << existing.specialty() << ")\n\n";
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
        {"Specialty", specialty},
        {"Room", room},
        {"Consultation fee", Formatter::money(fee)},
        {"Daily limit", std::to_string(dailyLimit)},
    };
    int bw = tui_.bannerOpen("CONFIRM DOCTOR DETAILS",
                             "review before registering",
                             {"Home", "Doctors", "Add", "Confirm"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";

    if (!tui_.confirm("Confirm registration?")) {
        tui_.toast("Registration cancelled.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    Doctor doctor(hospital_.nextDoctorId(), name, age, sex, contact, address,
                  specialty, room, fee, dailyLimit);
    hospital_.doctors().push_back(doctor);
    hospital_.saveAll();

    lastDoctorRegistration_ = std::chrono::steady_clock::now();
    logAudit(hospital_.dataDir(), "DOCTOR_REGISTERED",
             "id=" + std::to_string(doctor.id()) + " name=" + name +
             " specialty=" + specialty + " fee=" + std::to_string(fee));

    tui_.toast("Registered Dr. " + name + " (id #" +
               std::to_string(doctor.id()) + ").", Tui::Level::Success);
    tui_.pause();
}

void DoctorsModule::editDoctor() {
    tui_.clearScreen();
    tui_.banner("EDIT DOCTOR", "", {"Home", "Doctors", "Edit"});
    std::cout << "\n";

    if (hospital_.doctors().empty()) {
        tui_.toast("No doctors to edit.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    std::vector<std::string> dHeaders{"ID", "Name", "Specialty"};
    std::vector<std::vector<std::string>> dRows;
    for (const auto& d : hospital_.doctors()) {
        dRows.push_back({
            std::to_string(d.id()), "Dr. " + d.name(), d.specialty(),
        });
    }
    int dw = tui_.bannerOpen("SELECT DOCTOR TO EDIT", "",
                             {"Home", "Doctors", "Edit"},
                             tui_.tableBoxWidth(dHeaders, dRows));
    tui_.tableInBox(dw, dHeaders, dRows);
    std::cout << "\n";
    int id = validation_.readInt("Doctor id", 1, 1000000);
    Doctor* doctor = hospital_.findDoctor(id);
    if (!doctor) throw NotFoundException("doctor #" + std::to_string(id));

    std::cout << "\n  Editing: Dr. " << doctor->name() << "\n\n  "
              << Tui::Color::DIM
              << "Leave a value blank to keep it unchanged."
              << Tui::Color::RESET << "\n";

    std::string newName = validation_.readLine("Name [" + doctor->name() + "]", true);
    if (!newName.empty() && newName.size() < 2) {
        tui_.toast("Name must be at least 2 characters.", Tui::Level::Warning);
        tui_.pause();
        return;
    }

    std::string newContact = validation_.readLine(
        "Contact [" + doctor->contact() + "]", true);
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
        for (const auto& existing : hospital_.doctors()) {
            if (existing.id() == doctor->id()) continue;
            if (existing.contact() == newContact) {
                tui_.toast("Another doctor already uses this contact number.", Tui::Level::Warning);
                tui_.pause();
                return;
            }
        }
    }

    std::string newAddress = validation_.readLine(
        "Address [" + doctor->address() + "]", true);
    if (!newAddress.empty() && newAddress.size() < 10) {
        tui_.toast("Address must be at least 10 characters.", Tui::Level::Warning);
        tui_.pause();
        return;
    }

    std::string newSpecialty = validation_.readLine(
        "Specialty [" + doctor->specialty() + "]", true);
    if (!newSpecialty.empty() && Validator::isBlank(newSpecialty)) {
        tui_.toast("Specialty cannot be blank.", Tui::Level::Warning);
        tui_.pause();
        return;
    }

    std::string newRoom = validation_.readLine(
        "Consulting room [" + doctor->room() + "]", true);
    if (!newRoom.empty() && Validator::isBlank(newRoom)) {
        tui_.toast("Consulting room cannot be blank.", Tui::Level::Warning);
        tui_.pause();
        return;
    }

    std::string feeStr = validation_.readLine(
        "Consultation fee [" + Formatter::money(doctor->consultFee()) + "]", true);
    double newFee = doctor->consultFee();
    if (!feeStr.empty()) {
        try {
            std::size_t pos = 0;
            double v = std::stod(feeStr, &pos);
            if (pos != feeStr.size() || v < 0.0 || v > 1000000.0) {
                tui_.toast("Fee must be between 0 and 1,000,000.", Tui::Level::Warning);
                tui_.pause();
                return;
            }
            newFee = v;
        } catch (const std::exception&) {
            tui_.toast("Not a valid number.", Tui::Level::Warning);
            tui_.pause();
            return;
        }
    }

    std::string limitStr = validation_.readLine(
        "Daily limit [" + std::to_string(doctor->dailyAppointmentLimit()) + "]", true);
    int newLimit = doctor->dailyAppointmentLimit();
    if (!limitStr.empty()) {
        try {
            std::size_t pos = 0;
            int v = std::stoi(limitStr, &pos);
            if (pos != limitStr.size() || v < 1 || v > 50) {
                tui_.toast("Daily limit must be between 1 and 50.", Tui::Level::Warning);
                tui_.pause();
                return;
            }
            newLimit = v;
        } catch (const std::exception&) {
            tui_.toast("Not a valid number.", Tui::Level::Warning);
            tui_.pause();
            return;
        }
    }

    tui_.clearScreen();
    std::vector<std::string> headers{"Field", "Old Value", "New Value"};
    std::vector<std::vector<std::string>> rows;
    bool hasChanges = false;

    if (!newName.empty()) {
        rows.push_back({"Name", doctor->name(), newName});
        hasChanges = true;
    }
    if (!newContact.empty()) {
        rows.push_back({"Contact", doctor->contact(), newContact});
        hasChanges = true;
    }
    if (!newAddress.empty()) {
        rows.push_back({"Address", doctor->address(), newAddress});
        hasChanges = true;
    }
    if (!newSpecialty.empty()) {
        rows.push_back({"Specialty", doctor->specialty(), newSpecialty});
        hasChanges = true;
    }
    if (!newRoom.empty()) {
        rows.push_back({"Room", doctor->room(), newRoom});
        hasChanges = true;
    }
    if (newFee != doctor->consultFee()) {
        rows.push_back({"Fee", Formatter::money(doctor->consultFee()), Formatter::money(newFee)});
        hasChanges = true;
    }
    if (newLimit != doctor->dailyAppointmentLimit()) {
        rows.push_back({"Daily limit", std::to_string(doctor->dailyAppointmentLimit()),
                        std::to_string(newLimit)});
        hasChanges = true;
    }
    if (!hasChanges) {
        tui_.toast("No changes made.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    int bw = tui_.bannerOpen("CONFIRM CHANGES",
                             "review before saving",
                             {"Home", "Doctors", "Edit", "Confirm"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";

    if (!tui_.confirm("Save changes?")) {
        tui_.toast("No changes saved.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    if (!newName.empty()) doctor->setName(newName);
    if (!newContact.empty()) doctor->setContact(newContact);
    if (!newAddress.empty()) doctor->setAddress(newAddress);
    if (!newSpecialty.empty()) doctor->setSpecialty(newSpecialty);
    if (!newRoom.empty()) doctor->setRoom(newRoom);
    if (newFee != doctor->consultFee()) doctor->setConsultFee(newFee);
    if (newLimit != doctor->dailyAppointmentLimit()) doctor->setDailyAppointmentLimit(newLimit);

    hospital_.saveAll();
    logAudit(hospital_.dataDir(), "DOCTOR_UPDATED",
             "id=" + std::to_string(doctor->id()) + " name=" + doctor->name());
    tui_.toast("Updated.", Tui::Level::Success);
    tui_.pause();
}

void DoctorsModule::deleteDoctor() {
    tui_.clearScreen();
    tui_.banner("DELETE DOCTOR", "", {"Home", "Doctors", "Delete"});
    std::cout << "\n";

    if (hospital_.doctors().empty()) {
        tui_.toast("No doctors to delete.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    std::vector<std::string> dHeaders{"ID", "Name", "Specialty"};
    std::vector<std::vector<std::string>> dRows;
    for (const auto& d : hospital_.doctors()) {
        dRows.push_back({
            std::to_string(d.id()), "Dr. " + d.name(), d.specialty(),
        });
    }
    int dw = tui_.bannerOpen("SELECT DOCTOR TO DELETE", "",
                             {"Home", "Doctors", "Delete"},
                             tui_.tableBoxWidth(dHeaders, dRows));
    tui_.tableInBox(dw, dHeaders, dRows);
    std::cout << "\n";
    int id = validation_.readInt("Doctor id", 1, 1000000);
    Doctor* doctor = hospital_.findDoctor(id);
    if (!doctor) throw NotFoundException("doctor #" + std::to_string(id));

    int assignedPatients = 0;
    for (const auto& p : hospital_.patients()) {
        if (p.doctorId() == doctor->id()) ++assignedPatients;
    }
    if (assignedPatients > 0) {
        std::cout << "\n  " << Tui::Color::YELLOW
                  << "[!] This doctor has " << assignedPatients
                  << " assigned patient(s)." << Tui::Color::RESET << "\n\n";
        if (!tui_.confirm("Proceed with deletion? Patient doctor assignments will be cleared.")) {
            tui_.toast("Deletion cancelled.", Tui::Level::Info);
            tui_.pause();
            return;
        }
        for (auto& p : hospital_.patients()) {
            if (p.doctorId() == doctor->id()) p.setDoctorId(-1);
        }
    }

    int activeAppointments = 0;
    for (const auto& appt : hospital_.appointments()) {
        if (appt.doctorId() == doctor->id() &&
            appt.status() != AppointmentStatus::Cancelled) {
            ++activeAppointments;
        }
    }
    if (activeAppointments > 0) {
        std::cout << "\n  " << Tui::Color::YELLOW
                  << "[!] This doctor has " << activeAppointments
                  << " active appointment(s)." << Tui::Color::RESET << "\n\n";
        if (!tui_.confirm("Proceed with deletion? Appointments will be cancelled.")) {
            tui_.toast("Deletion cancelled.", Tui::Level::Info);
            tui_.pause();
            return;
        }
        for (auto& appt : hospital_.appointments()) {
            if (appt.doctorId() == doctor->id() &&
                appt.status() != AppointmentStatus::Cancelled) {
                appt.setStatus(AppointmentStatus::Cancelled);
            }
        }
    }

    std::cout << "\n  About to delete: Dr. " << doctor->name()
              << " (id #" << doctor->id() << ")\n\n";
    if (!tui_.confirm("Confirm deletion? This cannot be undone.")) {
        tui_.toast("Deletion cancelled.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    std::string deletedName = doctor->name();
    auto& doctors = hospital_.doctors();
    doctors.erase(std::remove_if(doctors.begin(), doctors.end(),
        [id](const Doctor& d) { return d.id() == id; }), doctors.end());
    hospital_.saveAll();
    logAudit(hospital_.dataDir(), "DOCTOR_DELETED",
             "id=" + std::to_string(id) + " name=" + deletedName);
    tui_.toast("Deleted Dr. " + deletedName + ".", Tui::Level::Success);
    tui_.pause();
}

void DoctorsModule::listDoctors() {
    std::string query;
    while (true) {
        tui_.clearScreen();
        auto filtered = filterDoctors(hospital_.doctors(), query);
        if (filtered.empty()) {
            tui_.toast("No doctors found.", Tui::Level::Info);
            tui_.pause();
            query.clear();
            continue;
        }
        std::string title = query.empty() ? "ALL DOCTORS"
            : "DOCTORS MATCHING \"" + query + "\"";
        showDoctorList(filtered, title + " (" + std::to_string(filtered.size()) + " found)");
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

int DoctorsModule::doctorWorkload(int doctorId) {
    int count = 0;
    std::time_t today = Validator::today();
    for (const auto& appointment : hospital_.appointments()) {
        if (appointment.doctorId() != doctorId) continue;
        if (appointment.status() == AppointmentStatus::Cancelled) continue;
        if (appointment.date() != today) continue;
        ++count;
    }
    for (const auto& patient : hospital_.patients()) {
        if (patient.doctorId() == doctorId) ++count;
    }
    return count;
}

constexpr int HUNGARIAN_INF = std::numeric_limits<int>::max() / 4;

DoctorsModule::HungarianResult DoctorsModule::solveHungarian(const std::vector<std::vector<int>>& cost) {
    int n = static_cast<int>(cost.size());
    int m = static_cast<int>(cost[0].size());

    std::vector<int> u(n + 1, 0);
    std::vector<int> v(m + 1, 0);
    std::vector<int> p(m + 1, 0);
    std::vector<int> way(m + 1, 0);

    for (int i = 1; i <= n; ++i) {
        p[0] = i;
        int j0 = 0;
        std::vector<int> minValue(m + 1, HUNGARIAN_INF);
        std::vector<bool> used(m + 1, false);
        do {
            used[j0] = true;
            int i0 = p[j0];
            int delta = HUNGARIAN_INF;
            int j1 = -1;
            for (int j = 1; j <= m; ++j) {
                if (used[j]) continue;
                int candidate = cost[i0 - 1][j - 1] - u[i0] - v[j];
                if (candidate < minValue[j]) {
                    minValue[j] = candidate;
                    way[j] = j0;
                }
                if (minValue[j] < delta) {
                    delta = minValue[j];
                    j1 = j;
                }
            }
            for (int j = 0; j <= m; ++j) {
                if (used[j]) {
                    u[p[j]] += delta;
                    v[j]    -= delta;
                } else {
                    minValue[j] -= delta;
                }
            }
            j0 = j1;
        } while (p[j0] != 0);

        do {
            int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0 != 0);
    }

    HungarianResult result;
    result.assignment.assign(n, -1);
    for (int j = 1; j <= m; ++j) {
        if (p[j] > 0 && p[j] <= n) {
            result.assignment[p[j] - 1] = j - 1;
        }
    }
    result.totalCost = 0;
    for (int i = 0; i < n; ++i) {
        if (result.assignment[i] >= 0) {
            result.totalCost += cost[i][result.assignment[i]];
        }
    }
    return result;
}

void DoctorsModule::autoAssignDoctors() {
    tui_.clearScreen();

    std::vector<Patient*> waiting;
    for (auto& patient : hospital_.patients()) {
        if (patient.hasDoctor()) continue;
        if (patient.isAdmitted() && patient.severity() == SEVERITY_NON_URGENT) continue;
        waiting.push_back(&patient);
    }

    if (waiting.empty()) {
        tui_.toast("No patients are waiting for a doctor assignment.",
                   Tui::Level::Info);
        tui_.pause();
        return;
    }
    if (hospital_.doctors().empty())
        throw NotFoundException("no doctors registered");

    std::sort(waiting.begin(), waiting.end(),
              [](Patient* a, Patient* b) {
                  return a->severity() > b->severity();
              });

    int n = static_cast<int>(waiting.size());
    int m = static_cast<int>(hospital_.doctors().size());
    int size = std::max(n, m);

    std::vector<std::vector<int>> cost(size, std::vector<int>(size, 0));
    std::vector<int> doctorWorkloads(m);
    for (int j = 0; j < m; ++j) {
        doctorWorkloads[j] = doctorWorkload(hospital_.doctors()[j].id());
    }

    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            if (i < n && j < m) {
                int severity = waiting[i]->severity();
                int workload = doctorWorkloads[j];
                int doctorLimit = hospital_.doctors()[j].dailyAppointmentLimit();
                if (workload >= doctorLimit) {
                    cost[i][j] = HUNGARIAN_INF / 2;
                } else {
                    cost[i][j] = (6 - severity) * (1 + workload);
                }
            } else {
                cost[i][j] = HUNGARIAN_INF / 4;
            }
        }
    }

    HungarianResult result = solveHungarian(cost);

    std::vector<std::vector<std::string>> rows;
    int realAssignments = 0;
    int realCost = 0;
    for (int i = 0; i < n; ++i) {
        int j = result.assignment[i];
        if (j < 0 || j >= m) continue;
        if (cost[i][j] >= HUNGARIAN_INF / 4) continue;
        Patient* patient = waiting[i];
        Doctor& doctor = hospital_.doctors()[j];
        rows.push_back({
            std::to_string(patient->id()),
            patient->name(),
            std::string(severityColor(patient->severity())) +
                severityLabel(patient->severity()) +
                Tui::Color::RESET,
            doctor.name() + " (" + doctor.specialty() + ")",
            std::to_string(doctorWorkloads[j]),
            std::to_string(cost[i][j]),
        });
        ++realAssignments;
        realCost += cost[i][j];
    }
    {
        std::vector<std::string> h{ "PID", "Patient", "Severity", "Doctor", "Doc. workload", "Cost" };
        int bw = tui_.bannerOpen(
            "AUTO-ASSIGN DOCTORS TO PATIENTS",
            "balanced assignment based on workload and severity",
            {"Home", "Doctors", "Auto-assign"},
            tui_.tableBoxWidth(h, rows, "Proposed assignments"));
        tui_.tableInBox(bw, h, rows, "Proposed assignments");
    }

    std::vector<std::string> sHeaders{"Metric", "Value"};
    std::vector<std::vector<std::string>> sRows{
        {"Total cost", std::to_string(realCost)},
        {"Assigned", std::to_string(realAssignments) + "/" + std::to_string(n)},
    };
    int sw = tui_.bannerOpen("ASSIGNMENT SUMMARY", "",
                             {"Home", "Doctors", "Auto-assign"},
                             tui_.tableBoxWidth(sHeaders, sRows));
    tui_.tableInBox(sw, sHeaders, sRows);
    std::cout << "\n";

    if (!tui_.confirm("Apply these assignments?")) {
        tui_.toast("No changes.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    for (int i = 0; i < n; ++i) {
        int j = result.assignment[i];
        if (j < 0 || j >= m) continue;
        if (cost[i][j] >= HUNGARIAN_INF / 4) continue;
        waiting[i]->setDoctorId(hospital_.doctors()[j].id());
    }
    hospital_.saveAll();
    logAudit(hospital_.dataDir(), "DOCTORS_AUTO_ASSIGNED",
             std::to_string(realAssignments) + " patients assigned");
    tui_.toast(std::to_string(realAssignments) +
               " patient(s) assigned.", Tui::Level::Success);
    tui_.pause();
}


void DoctorsModule::run() {
    while (true) {
        char choice = tui_.menu(
            "DOCTOR RECORDS",
            {"Home", "Doctors"},
            {
                { '1', "Register doctor",     "add a new doctor" },
                { '2', "Edit doctor",         "update an existing record" },
                { '3', "Delete doctor",       "remove a doctor record" },
                { '4', "List all doctors",    "search and view" },
                { '5', "Auto-assign doctors", "balance workload across staff" },
                { 'B', "Back",                "return to main menu" },
            });
        switch (choice) {
            case '1': addDoctor();        break;
            case '2': editDoctor();       break;
            case '3': deleteDoctor();     break;
            case '4': listDoctors();      break;
            case '5': autoAssignDoctors(); break;
            case 'B': return;
        }
    }
}

}
