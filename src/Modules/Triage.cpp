#include <hms/Core/Exceptions.h>
#include <hms/Core/Tui.h>
#include <hms/Core/Validation.h>
#include <hms/Hospital.h>
#include <hms/Models/Appointment.h>
#include <hms/Models/Patient.h>
#include <hms/Modules/Triage.h>

#include <algorithm>
#include <iostream>
#include <queue>
#include <sstream>
#include <vector>

namespace hms {

TriageModule::TriageQueue TriageModule::buildQueueFromHospital() {
    TriageQueue queue;
    for (const auto& patient : hospital_.patients()) {
        if (patient.isAdmitted()) continue;
        if (patient.hasDoctor()) continue;
        queue.push({ patient.id(), patient.severity(), patient.arrival() });
    }
    return queue;
}

std::vector<TriageModule::TriageEntry> TriageModule::snapshot(TriageQueue queue) {
    std::vector<TriageEntry> ordered;
    while (!queue.empty()) {
        ordered.push_back(queue.top());
        queue.pop();
    }
    return ordered;
}

void TriageModule::showQueue() {
    tui_.clearScreen();

    auto ordered = snapshot(buildQueueFromHospital());
    std::vector<std::string> headers{"Rank", "PID", "Name", "Age/Sex", "Severity", "Arrived"};
    std::vector<std::vector<std::string>> rows;
    int position = 1;
    for (const auto& entry : ordered) {
        Patient* patient = hospital_.findPatient(entry.patientId);
        if (!patient) continue;
        rows.push_back({
            "#" + std::to_string(position++),
            std::to_string(patient->id()),
            patient->name(),
            std::to_string(patient->age()) + " " + std::string(1, patient->sex()),
            std::string(severityColor(entry.severity)) + severityLabel(entry.severity) + Tui::Color::RESET,
            Validator::formatDateTime(entry.arrival),
        });
    }

    int bw = tui_.bannerOpen("EMERGENCY ROOM QUEUE", "patients ordered by severity, then arrival",
                             {"Home", "Emergency Room", "Queue"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    tui_.hintBar({"Total waiting: " + std::to_string(ordered.size())});
    std::cout << "\n";
    tui_.pause();
}

void TriageModule::admitPatient() {
    tui_.clearScreen();
    tui_.banner("ADMIT PATIENT TO ER QUEUE", "", {"Home", "Emergency Room", "Admit"});
    std::cout << "\n";

    auto ordered = snapshot(buildQueueFromHospital());
    if (!ordered.empty()) {
        std::vector<std::string> qHeaders{"Rank", "PID", "Name", "Age", "Sex", "Severity", "Status"};
        std::vector<std::vector<std::string>> qRows;
        int position = 1;
        for (const auto& entry : ordered) {
            Patient* p = hospital_.findPatient(entry.patientId);
            if (!p) continue;
            std::string sx = (p->sex() == 'M' || p->sex() == 'm') ? "Male" : "Female";
            std::string st = p->isAdmitted() ? "admitted" : p->hasDoctor() ? "with doctor" : "waiting";
            qRows.push_back({
                "#" + std::to_string(position++),
                std::to_string(p->id()),
                p->name(),
                std::to_string(p->age()),
                sx,
                std::string(severityColor(entry.severity)) + severityLabel(entry.severity) + Tui::Color::RESET,
                st,
            });
        }
        int qw = tui_.bannerOpen("CURRENT ER QUEUE", "patients waiting for treatment",
                                 {"Home", "Emergency Room", "Admit"},
                                 tui_.tableBoxWidth(qHeaders, qRows));
        tui_.tableInBox(qw, qHeaders, qRows);
        std::cout << "\n";
    } else {
        std::cout << "  " << Tui::Color::DIM << "ER queue is currently empty." << Tui::Color::RESET << "\n\n";
    }

    std::string name    = validation_.readName("Patient name");
    int age             = validation_.readInt("Age", 0, 130);
    char sex            = validation_.readChar("Sex (M/F)", "MF");
    std::string contact = validation_.readContact("Contact number");
    std::string address = validation_.readAddress("Address");
    int severity = validation_.readInt(
        "Initial severity (1-5, 1 if walk-in)", 1, 5);

    char senior     = (age >= 60) ? 'Y' : 'N';
    std::cout << "  Senior citizen: " << (senior == 'Y' ? "Yes" : "No")
              << " (auto-detected from age)\n";
    char pwd        = validation_.readChar("PWD? (Y/N)",                 "YN");
    char philhealth = validation_.readChar("Has PhilHealth? (Y/N)",      "YN");
    std::string promoCode = validation_.readLine("Promo code (optional)", true);
    if (!Validator::isBlank(promoCode)) {
        /* keep as-is */
    } else if (!promoCode.empty()) {
        tui_.toast("Promo code is whitespace-only; treating as empty.", Tui::Level::Warning);
        promoCode.clear();
    }

    for (const auto& existing : hospital_.patients()) {
        if (existing.contact() == contact) {
            std::cout << "\n  " << Tui::Color::YELLOW
                      << "[!] A patient with this contact number already exists:"
                      << Tui::Color::RESET << "\n"
                      << "      #" << existing.id() << "  "
                      << existing.name() << " (" << existing.age()
                      << existing.sex() << ")\n\n";
            if (tui_.confirm("Cancel admission?")) {
                tui_.toast("Admission cancelled.", Tui::Level::Info);
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
                             "review before admitting to ER",
                             {"Home", "Emergency Room", "Admit", "Confirm"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";

    if (!tui_.confirm("Confirm admission?")) {
        tui_.toast("Admission cancelled.", Tui::Level::Info);
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

    tui_.toast("Admitted " + patient.name()
               + " — id #" + std::to_string(patient.id())
               + ", severity " + severityLabel(severity),
               Tui::Level::Success);
    tui_.pause();
}

void TriageModule::callNextPatient() {
    tui_.clearScreen();
    tui_.banner("CALL NEXT PATIENT", "", {"Home", "Emergency Room", "Call Next"});

    auto queue = buildQueueFromHospital();
    if (queue.empty()) {
        tui_.toast("ER queue is empty.", Tui::Level::Info);
        tui_.pause();
        return;
    }
    TriageEntry next = queue.top();
    Patient* patient = hospital_.findPatient(next.patientId);
    if (!patient) {
        tui_.toast("Internal error: patient missing.", Tui::Level::Error);
        tui_.pause();
        return;
    }

    std::string sx = (patient->sex() == 'M' || patient->sex() == 'm') ? "Male" : "Female";
    std::vector<std::string> cHeaders{"ID", "Name", "Age", "Sex", "Severity", "Arrived"};
    std::vector<std::vector<std::string>> cRows{{
        std::to_string(patient->id()), patient->name(),
        std::to_string(patient->age()), sx,
        std::string(severityColor(next.severity)) + severityLabel(next.severity) + Tui::Color::RESET,
        Validator::formatDateTime(next.arrival),
    }};
    int cw = tui_.bannerOpen("NOW CALLING", "",
                             {"Home", "Emergency Room", "Call Next"},
                             tui_.tableBoxWidth(cHeaders, cRows));
    tui_.tableInBox(cw, cHeaders, cRows);
    std::cout << "\n";

    if (!tui_.confirm("Mark this patient as called (remove from queue)?")) {
        tui_.toast("No change.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    int doctorId = -1;
    if (!hospital_.doctors().empty()) {
        std::vector<std::string> dHeaders{"ID", "Name", "Specialty", "Today's load"};
        std::vector<std::vector<std::string>> dRows;
        std::time_t today = Validator::today();
        for (const auto& doctor : hospital_.doctors()) {
            int load = 0;
            for (const auto& appointment : hospital_.appointments()) {
                if (appointment.doctorId() != doctor.id()) continue;
                if (appointment.status() == AppointmentStatus::Cancelled) continue;
                if (appointment.date() != today) continue;
                ++load;
            }
            for (const auto& p : hospital_.patients()) {
                if (p.doctorId() == doctor.id()) ++load;
            }
            std::string loadCell = std::to_string(load) + "/" + std::to_string(doctor.dailyAppointmentLimit());
            if (load >= doctor.dailyAppointmentLimit()) {
                loadCell = std::string(Tui::Color::WHITE) + loadCell + " (full)" + Tui::Color::RESET;
            }
            dRows.push_back({
                std::to_string(doctor.id()), doctor.name(), doctor.specialty(), loadCell,
            });
        }
        int dw = tui_.bannerOpen("ASSIGN ATTENDING DOCTOR", "id, or 0 to skip",
                                 {"Home", "Emergency Room", "Call Next"},
                                 tui_.tableBoxWidth(dHeaders, dRows));
        tui_.tableInBox(dw, dHeaders, dRows);
        std::cout << "\n";
        doctorId = validation_.readInt("Doctor id (0 = skip)", 0, 1000000);
        if (doctorId > 0) {
            Doctor* doctor = hospital_.findDoctor(doctorId);
            if (!doctor) {
                tui_.toast("Doctor not found; skipping assignment.",
                           Tui::Level::Warning);
                doctorId = -1;
            } else {
                int load = 0;
                for (const auto& appointment : hospital_.appointments()) {
                    if (appointment.doctorId() != doctorId) continue;
                    if (appointment.status() == AppointmentStatus::Cancelled) continue;
                    if (appointment.date() != today) continue;
                    ++load;
                }
                for (const auto& p : hospital_.patients()) {
                    if (p.doctorId() == doctorId) ++load;
                }
                if (load >= doctor->dailyAppointmentLimit()) {
                    std::cout << "\n  " << Tui::Color::YELLOW
                              << "[!] Dr. " << doctor->name() << " is at capacity ("
                              << load << "/" << doctor->dailyAppointmentLimit() << ")."
                              << Tui::Color::RESET << "\n\n";
                    if (!tui_.confirm("Assign anyway?")) {
                        doctorId = -1;
                    }
                }
            }
        }
    }
    patient->setDoctorId(doctorId > 0 ? doctorId : -1);
    hospital_.saveAll();

    tui_.toast("Patient called from queue.", Tui::Level::Success);
    tui_.pause();
}

void TriageModule::changeSeverity() {
    tui_.clearScreen();
    tui_.banner("UPDATE PATIENT SEVERITY", "", {"Home", "Emergency Room", "Update Severity"});
    std::cout << "\n";

    auto ordered = snapshot(buildQueueFromHospital());
    if (ordered.empty()) {
        tui_.toast("ER queue is empty.", Tui::Level::Info);
        tui_.pause();
        return;
    }
    std::vector<std::string> pHeaders{"ID", "Name", "Age", "Sex", "Severity", "Status"};
    std::vector<std::vector<std::string>> pRows;
    for (const auto& entry : ordered) {
        Patient* p = hospital_.findPatient(entry.patientId);
        if (!p) continue;
        std::string sx = (p->sex() == 'M' || p->sex() == 'm') ? "Male" : "Female";
        std::string st = p->isAdmitted() ? "admitted" : p->hasDoctor() ? "with doctor" : "waiting";
        pRows.push_back({
            std::to_string(p->id()), p->name(), std::to_string(p->age()), sx,
            std::string(severityColor(entry.severity)) + severityLabel(entry.severity) + Tui::Color::RESET,
            st,
        });
    }
    int pw = tui_.bannerOpen("SELECT PATIENT", "",
                             {"Home", "Emergency Room", "Update Severity"},
                             tui_.tableBoxWidth(pHeaders, pRows));
    tui_.tableInBox(pw, pHeaders, pRows);
    std::cout << "\n";
    int id = validation_.readInt("Patient id", 1, 1000000);
    Patient* patient = hospital_.findPatient(id);
    if (!patient) throw NotFoundException("patient #" + std::to_string(id));
    if (patient->isAdmitted() || patient->hasDoctor()) {
        tui_.toast("Patient is no longer in ER queue.", Tui::Level::Warning);
        tui_.pause();
        return;
    }
    std::cout << "\n  Current severity: "
              << severityColor(patient->severity())
              << severityLabel(patient->severity()) << Tui::Color::RESET << "\n";
    int level = validation_.readInt("New severity (1-5)", 1, 5);
    patient->setSeverity(level);
    hospital_.saveAll();
    tui_.toast("Updated.", Tui::Level::Success);
    tui_.pause();
}


void TriageModule::run() {
    while (true) {
        char choice = tui_.menu(
            "EMERGENCY ROOM",
            {"Home", "Emergency Room"},
            {
                { '1', "View ER queue",            "show patients ordered by severity" },
                { '2', "Admit new patient",        "add patient to queue" },
                { '3', "Call next patient",        "take highest priority next" },
                { '4', "Update severity",          "change a patient's level" },
                { 'B', "Back",                     "return to main menu" },
            });
        switch (choice) {
            case '1': showQueue();       break;
            case '2': admitPatient();    break;
            case '3': callNextPatient(); break;
            case '4': changeSeverity();  break;
            case 'B': return;
        }
    }
}

}
