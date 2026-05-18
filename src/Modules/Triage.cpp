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

namespace hms::triage {

namespace {

struct TriageEntry {
    int patientId;
    int severity;
    std::time_t arrival;
};

struct HigherPriorityFirst {
    bool operator()(const TriageEntry& a, const TriageEntry& b) const {
        if (a.severity != b.severity) return a.severity < b.severity;
        return a.arrival > b.arrival;
    }
};

using TriageQueue =
    std::priority_queue<TriageEntry, std::vector<TriageEntry>, HigherPriorityFirst>;

TriageQueue buildQueueFromHospital(Hospital& hospital) {
    TriageQueue queue;
    for (const auto& patient : hospital.patients()) {
        if (patient.isAdmitted()) continue;
        if (patient.hasDoctor()) continue;
        queue.push({ patient.id(), patient.severity(), patient.arrival() });
    }
    return queue;
}

std::vector<TriageEntry> snapshot(TriageQueue queue) {
    std::vector<TriageEntry> ordered;
    while (!queue.empty()) {
        ordered.push_back(queue.top());
        queue.pop();
    }
    return ordered;
}

void showQueue(Hospital& hospital) {
    tui::clearScreen();

    auto ordered = snapshot(buildQueueFromHospital(hospital));
    std::vector<std::string> headers{"Rank", "PID", "Name", "Age/Sex", "Severity", "Arrived"};
    std::vector<std::vector<std::string>> rows;
    int position = 1;
    for (const auto& entry : ordered) {
        Patient* patient = hospital.findPatient(entry.patientId);
        if (!patient) continue;
        rows.push_back({
            "#" + std::to_string(position++),
            std::to_string(patient->id()),
            patient->name(),
            std::to_string(patient->age()) + " " + std::string(1, patient->sex()),
            std::string(severityColor(entry.severity)) + severityLabel(entry.severity) + tui::color::RESET,
            validation::formatDateTime(entry.arrival),
        });
    }

    int bw = tui::bannerOpen("EMERGENCY ROOM QUEUE", "patients ordered by severity, then arrival",
                             {"Home", "Emergency Room", "Queue"},
                             tui::tableBoxWidth(headers, rows));
    tui::tableInBox(bw, headers, rows);
    tui::hintBar({"Total waiting: " + std::to_string(ordered.size())});
    std::cout << "\n";
    tui::pause();
}

void admitPatient(Hospital& hospital) {
    tui::clearScreen();
    tui::banner("ADMIT PATIENT TO ER QUEUE", "", {"Home", "Emergency Room", "Admit"});
    std::cout << "\n";

    auto ordered = snapshot(buildQueueFromHospital(hospital));
    if (!ordered.empty()) {
        std::vector<std::string> qHeaders{"Rank", "PID", "Name", "Age", "Sex", "Severity", "Status"};
        std::vector<std::vector<std::string>> qRows;
        int position = 1;
        for (const auto& entry : ordered) {
            Patient* p = hospital.findPatient(entry.patientId);
            if (!p) continue;
            std::string sx = (p->sex() == 'M' || p->sex() == 'm') ? "Male" : "Female";
            std::string st = p->isAdmitted() ? "admitted" : p->hasDoctor() ? "with doctor" : "waiting";
            qRows.push_back({
                "#" + std::to_string(position++),
                std::to_string(p->id()),
                p->name(),
                std::to_string(p->age()),
                sx,
                std::string(severityColor(entry.severity)) + severityLabel(entry.severity) + tui::color::RESET,
                st,
            });
        }
        int qw = tui::bannerOpen("CURRENT ER QUEUE", "patients waiting for treatment",
                                 {"Home", "Emergency Room", "Admit"},
                                 tui::tableBoxWidth(qHeaders, qRows));
        tui::tableInBox(qw, qHeaders, qRows);
        std::cout << "\n";
    } else {
        std::cout << "  " << tui::color::DIM << "ER queue is currently empty." << tui::color::RESET << "\n\n";
    }

    std::string name    = validation::readName("Patient name");
    int age             = validation::readInt("Age", 0, 130);
    char sex            = validation::readChar("Sex (M/F)", "MF");
    std::string contact = validation::readContact("Contact number");
    std::string address = validation::readAddress("Address");
    int severity = validation::readInt(
        "Initial severity (1-5, 1 if walk-in)", 1, 5);

    char senior     = (age >= 60) ? 'Y' : 'N';
    std::cout << "  Senior citizen: " << (senior == 'Y' ? "Yes" : "No")
              << " (auto-detected from age)\n";
    char pwd        = validation::readChar("PWD? (Y/N)",                 "YN");
    char philhealth = validation::readChar("Has PhilHealth? (Y/N)",      "YN");
    std::string promoCode = validation::readLine("Promo code (optional)", true);
    if (!validation::isBlank(promoCode)) {
        /* keep as-is */
    } else if (!promoCode.empty()) {
        tui::toast("Promo code is whitespace-only; treating as empty.", tui::Level::Warning);
        promoCode.clear();
    }

    for (const auto& existing : hospital.patients()) {
        if (existing.contact() == contact) {
            std::cout << "\n  " << tui::color::YELLOW
                      << "[!] A patient with this contact number already exists:"
                      << tui::color::RESET << "\n"
                      << "      #" << existing.id() << "  "
                      << existing.name() << " (" << existing.age()
                      << existing.sex() << ")\n\n";
            if (tui::confirm("Cancel admission?")) {
                tui::toast("Admission cancelled.", tui::Level::Info);
                tui::pause();
                return;
            }
            break;
        }
    }

    tui::clearScreen();
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
    int bw = tui::bannerOpen("CONFIRM PATIENT DETAILS",
                             "review before admitting to ER",
                             {"Home", "Emergency Room", "Admit", "Confirm"},
                             tui::tableBoxWidth(headers, rows));
    tui::tableInBox(bw, headers, rows);
    std::cout << "\n";

    if (!tui::confirm("Confirm admission?")) {
        tui::toast("Admission cancelled.", tui::Level::Info);
        tui::pause();
        return;
    }

    Patient patient(hospital.nextPatientId(), name, age, sex, contact, address);
    patient.setSeverity(severity);
    patient.setArrival(validation::now());
    patient.setSenior(senior == 'Y');
    patient.setPWD(pwd == 'Y');
    patient.setPhilHealth(philhealth == 'Y');
    patient.setPromoCode(promoCode);

    hospital.patients().push_back(patient);
    hospital.saveAll();

    tui::toast("Admitted " + patient.name()
               + " — id #" + std::to_string(patient.id())
               + ", severity " + severityLabel(severity),
               tui::Level::Success);
    tui::pause();
}

void callNextPatient(Hospital& hospital) {
    tui::clearScreen();
    tui::banner("CALL NEXT PATIENT", "", {"Home", "Emergency Room", "Call Next"});

    auto queue = buildQueueFromHospital(hospital);
    if (queue.empty()) {
        tui::toast("ER queue is empty.", tui::Level::Info);
        tui::pause();
        return;
    }
    TriageEntry next = queue.top();
    Patient* patient = hospital.findPatient(next.patientId);
    if (!patient) {
        tui::toast("Internal error: patient missing.", tui::Level::Error);
        tui::pause();
        return;
    }

    std::string sx = (patient->sex() == 'M' || patient->sex() == 'm') ? "Male" : "Female";
    std::vector<std::string> cHeaders{"ID", "Name", "Age", "Sex", "Severity", "Arrived"};
    std::vector<std::vector<std::string>> cRows{{
        std::to_string(patient->id()), patient->name(),
        std::to_string(patient->age()), sx,
        std::string(severityColor(next.severity)) + severityLabel(next.severity) + tui::color::RESET,
        validation::formatDateTime(next.arrival),
    }};
    int cw = tui::bannerOpen("NOW CALLING", "",
                             {"Home", "Emergency Room", "Call Next"},
                             tui::tableBoxWidth(cHeaders, cRows));
    tui::tableInBox(cw, cHeaders, cRows);
    std::cout << "\n";

    if (!tui::confirm("Mark this patient as called (remove from queue)?")) {
        tui::toast("No change.", tui::Level::Info);
        tui::pause();
        return;
    }

    int doctorId = -1;
    if (!hospital.doctors().empty()) {
        std::vector<std::string> dHeaders{"ID", "Name", "Specialty", "Today's load"};
        std::vector<std::vector<std::string>> dRows;
        std::time_t today = validation::today();
        for (const auto& doctor : hospital.doctors()) {
            int load = 0;
            for (const auto& appointment : hospital.appointments()) {
                if (appointment.doctorId() != doctor.id()) continue;
                if (appointment.status() == AppointmentStatus::Cancelled) continue;
                if (appointment.date() != today) continue;
                ++load;
            }
            for (const auto& p : hospital.patients()) {
                if (p.doctorId() == doctor.id()) ++load;
            }
            std::string loadCell = std::to_string(load) + "/" + std::to_string(doctor.dailyAppointmentLimit());
            if (load >= doctor.dailyAppointmentLimit()) {
                loadCell = std::string(tui::color::WHITE) + loadCell + " (full)" + tui::color::RESET;
            }
            dRows.push_back({
                std::to_string(doctor.id()), doctor.name(), doctor.specialty(), loadCell,
            });
        }
        int dw = tui::bannerOpen("ASSIGN ATTENDING DOCTOR", "id, or 0 to skip",
                                 {"Home", "Emergency Room", "Call Next"},
                                 tui::tableBoxWidth(dHeaders, dRows));
        tui::tableInBox(dw, dHeaders, dRows);
        std::cout << "\n";
        doctorId = validation::readInt("Doctor id (0 = skip)", 0, 1000000);
        if (doctorId > 0) {
            Doctor* doctor = hospital.findDoctor(doctorId);
            if (!doctor) {
                tui::toast("Doctor not found; skipping assignment.",
                           tui::Level::Warning);
                doctorId = -1;
            } else {
                int load = 0;
                for (const auto& appointment : hospital.appointments()) {
                    if (appointment.doctorId() != doctorId) continue;
                    if (appointment.status() == AppointmentStatus::Cancelled) continue;
                    if (appointment.date() != today) continue;
                    ++load;
                }
                for (const auto& p : hospital.patients()) {
                    if (p.doctorId() == doctorId) ++load;
                }
                if (load >= doctor->dailyAppointmentLimit()) {
                    std::cout << "\n  " << tui::color::YELLOW
                              << "[!] Dr. " << doctor->name() << " is at capacity ("
                              << load << "/" << doctor->dailyAppointmentLimit() << ")."
                              << tui::color::RESET << "\n\n";
                    if (!tui::confirm("Assign anyway?")) {
                        doctorId = -1;
                    }
                }
            }
        }
    }
    patient->setDoctorId(doctorId > 0 ? doctorId : -1);
    hospital.saveAll();

    tui::toast("Patient called from queue.", tui::Level::Success);
    tui::pause();
}

void changeSeverity(Hospital& hospital) {
    tui::clearScreen();
    tui::banner("UPDATE PATIENT SEVERITY", "", {"Home", "Emergency Room", "Update Severity"});
    std::cout << "\n";

    auto ordered = snapshot(buildQueueFromHospital(hospital));
    if (ordered.empty()) {
        tui::toast("ER queue is empty.", tui::Level::Info);
        tui::pause();
        return;
    }
    std::vector<std::string> pHeaders{"ID", "Name", "Age", "Sex", "Severity", "Status"};
    std::vector<std::vector<std::string>> pRows;
    for (const auto& entry : ordered) {
        Patient* p = hospital.findPatient(entry.patientId);
        if (!p) continue;
        std::string sx = (p->sex() == 'M' || p->sex() == 'm') ? "Male" : "Female";
        std::string st = p->isAdmitted() ? "admitted" : p->hasDoctor() ? "with doctor" : "waiting";
        pRows.push_back({
            std::to_string(p->id()), p->name(), std::to_string(p->age()), sx,
            std::string(severityColor(entry.severity)) + severityLabel(entry.severity) + tui::color::RESET,
            st,
        });
    }
    int pw = tui::bannerOpen("SELECT PATIENT", "",
                             {"Home", "Emergency Room", "Update Severity"},
                             tui::tableBoxWidth(pHeaders, pRows));
    tui::tableInBox(pw, pHeaders, pRows);
    std::cout << "\n";
    int id = validation::readInt("Patient id", 1, 1000000);
    Patient* patient = hospital.findPatient(id);
    if (!patient) throw NotFoundException("patient #" + std::to_string(id));
    if (patient->isAdmitted() || patient->hasDoctor()) {
        tui::toast("Patient is no longer in ER queue.", tui::Level::Warning);
        tui::pause();
        return;
    }
    std::cout << "\n  Current severity: "
              << severityColor(patient->severity())
              << severityLabel(patient->severity()) << tui::color::RESET << "\n";
    int level = validation::readInt("New severity (1-5)", 1, 5);
    patient->setSeverity(level);
    hospital.saveAll();
    tui::toast("Updated.", tui::Level::Success);
    tui::pause();
}

}

void run(Hospital& hospital) {
    while (true) {
        char choice = tui::menu(
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
            case '1': showQueue(hospital);       break;
            case '2': admitPatient(hospital);    break;
            case '3': callNextPatient(hospital); break;
            case '4': changeSeverity(hospital);  break;
            case 'B': return;
        }
    }
}

}
