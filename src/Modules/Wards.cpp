#include <hms/Core/Exceptions.h>
#include <hms/Core/Format.h>
#include <hms/Core/Tui.h>
#include <hms/Core/Validation.h>
#include <hms/Hospital.h>
#include <hms/Models/Appointment.h>
#include <hms/Models/Bed.h>
#include <hms/Models/Doctor.h>
#include <hms/Models/Patient.h>
#include <hms/Modules/Wards.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

namespace hms::wards {

namespace {

void showAllBeds(Hospital& hospital) {
    tui::clearScreen();

    std::vector<std::string> headers{ "Bed", "Ward", "Status", "Patient", "Days", "Daily rate" };
    std::vector<std::vector<std::string>> rows;
    std::time_t today = validation::today();
    for (const auto& bed : hospital.beds()) {
        std::string occupancyCell = bed.isFree()
            ? std::string(tui::color::WHITE) + "FREE" + tui::color::RESET
            : std::string(tui::color::WHITE) + "OCCUPIED" + tui::color::RESET;
        std::string patientCell = "-";
        std::string daysCell = "-";
        if (!bed.isFree()) {
            Patient* patient = hospital.findPatient(bed.occupantId());
            patientCell = patient ? patient->name() : ("#" + std::to_string(bed.occupantId()));
            daysCell = std::to_string(bed.daysOccupied(today));
        }
        rows.push_back({
            "#" + std::to_string(bed.id()), bed.ward(), occupancyCell,
            patientCell, daysCell, format::money(bed.dailyRate()),
        });
    }
    int bw = tui::bannerOpen("WARD STATUS", "", {"Home", "Ward & Beds", "View"},
                             tui::tableBoxWidth(headers, rows));
    tui::tableInBox(bw, headers, rows);
    std::cout << "\n";
    tui::pause();
}

void admitToBed(Hospital& hospital) {
    if (hospital.patients().empty())
        throw NotFoundException("no patients in the system");

    tui::clearScreen();
    tui::banner("ADMIT PATIENT TO WARD", "", {"Home", "Ward & Beds", "Admit"});
    std::cout << "\n";

    std::vector<std::string> pHeaders{"ID", "Name", "Age", "Sex", "Severity", "Status"};
    std::vector<std::vector<std::string>> pRows;
    for (const auto& p : hospital.patients()) {
        if (p.isAdmitted()) continue;
        std::string sx = (p.sex() == 'M' || p.sex() == 'm') ? "Male" : "Female";
        std::string st = p.hasDoctor() ? "with doctor" : "waiting";
        pRows.push_back({
            std::to_string(p.id()), p.name(), std::to_string(p.age()), sx,
            std::string(severityColor(p.severity())) + severityLabel(p.severity()) + tui::color::RESET,
            st,
        });
    }
    int pw = tui::bannerOpen("SELECT PATIENT", "",
                             {"Home", "Ward & Beds", "Admit"},
                             tui::tableBoxWidth(pHeaders, pRows));
    tui::tableInBox(pw, pHeaders, pRows);
    std::cout << "\n";
    int patientId = validation::readInt("Patient id", 1, 1000000);
    Patient* patient = hospital.findPatient(patientId);
    if (!patient) throw NotFoundException("patient #" + std::to_string(patientId));
    if (patient->isAdmitted())
        throw InvalidInputException(
            patient->name() + " is already in bed #" +
            std::to_string(patient->bedId()));

    std::vector<std::string> bHeaders{"Bed", "Ward", "Daily Rate", "Status"};
    std::vector<std::vector<std::string>> bRows;
    int freeCount = 0;
    for (const auto& bed : hospital.beds()) {
        if (!bed.isFree()) continue;
        bRows.push_back({
            "#" + std::to_string(bed.id()), bed.ward(),
            format::money(bed.dailyRate()), "FREE",
        });
        ++freeCount;
    }
    if (freeCount == 0)
        throw CapacityException("all beds are occupied");
    int bw = tui::bannerOpen("SELECT BED", "",
                             {"Home", "Ward & Beds", "Admit"},
                             tui::tableBoxWidth(bHeaders, bRows));
    tui::tableInBox(bw, bHeaders, bRows);
    std::cout << "\n";
    int bedId = validation::readInt("Bed id", 1, 1000000);
    Bed* bed = hospital.findBed(bedId);
    if (!bed) throw NotFoundException("bed #" + std::to_string(bedId));
    if (!bed->isFree()) throw CapacityException("bed is already occupied");

    bed->admit(patientId, validation::today());
    patient->setBedId(bedId);
    hospital.saveAll();

    tui::toast("Admitted " + patient->name() + " to bed #" +
               std::to_string(bedId) + " (" + bed->ward() + ").",
               tui::Level::Success);
    tui::pause();
}

void dischargeFromBed(Hospital& hospital) {
    tui::clearScreen();
    tui::banner("DISCHARGE PATIENT", "", {"Home", "Ward & Beds", "Discharge"});
    std::cout << "\n";

    std::time_t today = validation::today();
    std::vector<std::string> bHeaders{"Bed", "Ward", "Patient", "Days"};
    std::vector<std::vector<std::string>> bRows;
    for (const auto& b : hospital.beds()) {
        if (b.isFree()) continue;
        Patient* p = hospital.findPatient(b.occupantId());
        bRows.push_back({
            "#" + std::to_string(b.id()), b.ward(),
            p ? p->name() : "#" + std::to_string(b.occupantId()),
            std::to_string(b.daysOccupied(today)) + " day(s)",
        });
    }
    int bw = tui::bannerOpen("SELECT BED TO DISCHARGE", "",
                             {"Home", "Ward & Beds", "Discharge"},
                             tui::tableBoxWidth(bHeaders, bRows));
    tui::tableInBox(bw, bHeaders, bRows);
    std::cout << "\n";
    int bedId = validation::readInt("Bed id", 1, 1000000);
    Bed* bed = hospital.findBed(bedId);
    if (!bed) throw NotFoundException("bed #" + std::to_string(bedId));
    if (bed->isFree())
        throw InvalidInputException("bed is already free");

    int patientId = bed->occupantId();
    Patient* patient = hospital.findPatient(patientId);
    int days = bed->daysOccupied(validation::today());
    std::string name = patient ? patient->name() : "patient #" + std::to_string(patientId);

    std::cout << "\n  About to discharge " << name
              << " from bed #" << bedId
              << " after " << days << " day(s).\n";
    if (!tui::confirm("Confirm discharge?")) {
        tui::toast("Cancelled.", tui::Level::Info);
        tui::pause();
        return;
    }

    bed->discharge();
    if (patient) patient->setBedId(-1);
    hospital.saveAll();
    tui::toast("Discharged.", tui::Level::Success);
    tui::pause();
}

void showAdmittedPatients(Hospital& hospital) {
    tui::clearScreen();

    std::vector<std::string> headers{ "PID", "Name", "Ward", "Bed", "Doctor", "Days" };
    std::vector<std::vector<std::string>> rows;
    std::time_t today = validation::today();
    for (const auto& patient : hospital.patients()) {
        if (!patient.isAdmitted()) continue;
        Bed* bed = hospital.findBed(patient.bedId());
        Doctor* doctor = patient.hasDoctor()
            ? hospital.findDoctor(patient.doctorId()) : nullptr;
        rows.push_back({
            std::to_string(patient.id()), patient.name(),
            bed ? bed->ward() : "-",
            "#" + std::to_string(patient.bedId()),
            doctor ? doctor->name() : "-",
            bed ? std::to_string(bed->daysOccupied(today)) : "-",
        });
    }
    int bw = tui::bannerOpen("PATIENT CENSUS", "currently admitted",
                             {"Home", "Ward & Beds", "Census"},
                             tui::tableBoxWidth(headers, rows));
    tui::tableInBox(bw, headers, rows);
    std::cout << "\n";
    tui::pause();
}

}

void run(Hospital& hospital) {
    while (true) {
        char choice = tui::menu(
            "WARD & BEDS",
            {"Home", "Ward & Beds"},
            {
                { '1', "View bed status",      "occupied / free per ward" },
                { '2', "Admit to bed",         "place a patient in a bed" },
                { '3', "Discharge",            "free a bed" },
            { '4', "Patient census",       "all currently admitted" },
            { 'B', "Back",                 "return to main menu" },
        });
        switch (choice) {
            case '1': showAllBeds(hospital);          break;
            case '2': admitToBed(hospital);           break;
            case '3': dischargeFromBed(hospital);     break;
            case '4': showAdmittedPatients(hospital); break;
            case 'B': return;
        }
    }
}

}
