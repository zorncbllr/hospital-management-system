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

namespace hms {


void WardsModule::showAllBeds() {
    tui_.clearScreen();

    std::vector<std::string> headers{ "Bed", "Ward", "Status", "Patient", "Days", "Daily rate" };
    std::vector<std::vector<std::string>> rows;
    std::time_t today = Validator::today();
    for (const auto& bed : hospital_.beds()) {
        std::string occupancyCell = bed.isFree()
            ? std::string(Tui::Color::WHITE) + "FREE" + Tui::Color::RESET
            : std::string(Tui::Color::WHITE) + "OCCUPIED" + Tui::Color::RESET;
        std::string patientCell = "-";
        std::string daysCell = "-";
        if (!bed.isFree()) {
            Patient* patient = hospital_.findPatient(bed.occupantId());
            patientCell = patient ? patient->name() : ("#" + std::to_string(bed.occupantId()));
            daysCell = std::to_string(bed.daysOccupied(today));
        }
        rows.push_back({
            "#" + std::to_string(bed.id()), bed.ward(), occupancyCell,
            patientCell, daysCell, Formatter::money(bed.dailyRate()),
        });
    }
    int bw = tui_.bannerOpen("WARD STATUS", "", {"Home", "Ward & Beds", "View"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";
    tui_.pause();
}

void WardsModule::admitToBed() {
    if (hospital_.patients().empty())
        throw NotFoundException("no patients in the system");

    tui_.clearScreen();
    tui_.banner("ADMIT PATIENT TO WARD", "", {"Home", "Ward & Beds", "Admit"});
    std::cout << "\n";

    std::vector<std::string> pHeaders{"ID", "Name", "Age", "Sex", "Severity", "Status"};
    std::vector<std::vector<std::string>> pRows;
    for (const auto& p : hospital_.patients()) {
        if (p.isAdmitted()) continue;
        std::string sx = (p.sex() == 'M' || p.sex() == 'm') ? "Male" : "Female";
        std::string st = p.hasDoctor() ? "with doctor" : "waiting";
        pRows.push_back({
            std::to_string(p.id()), p.name(), std::to_string(p.age()), sx,
            std::string(severityColor(p.severity())) + severityLabel(p.severity()) + Tui::Color::RESET,
            st,
        });
    }
    int pw = tui_.bannerOpen("SELECT PATIENT", "",
                             {"Home", "Ward & Beds", "Admit"},
                             tui_.tableBoxWidth(pHeaders, pRows));
    tui_.tableInBox(pw, pHeaders, pRows);
    std::cout << "\n";
    int patientId = validation_.readInt("Patient id", 1, 1000000);
    Patient* patient = hospital_.findPatient(patientId);
    if (!patient) throw NotFoundException("patient #" + std::to_string(patientId));
    if (patient->isAdmitted())
        throw InvalidInputException(
            patient->name() + " is already in bed #" +
            std::to_string(patient->bedId()));

    std::vector<std::string> bHeaders{"Bed", "Ward", "Daily Rate", "Status"};
    std::vector<std::vector<std::string>> bRows;
    int freeCount = 0;
    for (const auto& bed : hospital_.beds()) {
        if (!bed.isFree()) continue;
        bRows.push_back({
            "#" + std::to_string(bed.id()), bed.ward(),
            Formatter::money(bed.dailyRate()), "FREE",
        });
        ++freeCount;
    }
    if (freeCount == 0)
        throw CapacityException("all beds are occupied");
    int bw = tui_.bannerOpen("SELECT BED", "",
                             {"Home", "Ward & Beds", "Admit"},
                             tui_.tableBoxWidth(bHeaders, bRows));
    tui_.tableInBox(bw, bHeaders, bRows);
    std::cout << "\n";
    int bedId = validation_.readInt("Bed id", 1, 1000000);
    Bed* bed = hospital_.findBed(bedId);
    if (!bed) throw NotFoundException("bed #" + std::to_string(bedId));
    if (!bed->isFree()) throw CapacityException("bed is already occupied");

    bed->admit(patientId, Validator::today());
    patient->setBedId(bedId);
    hospital_.saveAll();

    tui_.toast("Admitted " + patient->name() + " to bed #" +
               std::to_string(bedId) + " (" + bed->ward() + ").",
               Tui::Level::Success);
    tui_.pause();
}

void WardsModule::dischargeFromBed() {
    tui_.clearScreen();
    tui_.banner("DISCHARGE PATIENT", "", {"Home", "Ward & Beds", "Discharge"});
    std::cout << "\n";

    std::time_t today = Validator::today();
    std::vector<std::string> bHeaders{"Bed", "Ward", "Patient", "Days"};
    std::vector<std::vector<std::string>> bRows;
    for (const auto& b : hospital_.beds()) {
        if (b.isFree()) continue;
        Patient* p = hospital_.findPatient(b.occupantId());
        bRows.push_back({
            "#" + std::to_string(b.id()), b.ward(),
            p ? p->name() : "#" + std::to_string(b.occupantId()),
            std::to_string(b.daysOccupied(today)) + " day(s)",
        });
    }
    int bw = tui_.bannerOpen("SELECT BED TO DISCHARGE", "",
                             {"Home", "Ward & Beds", "Discharge"},
                             tui_.tableBoxWidth(bHeaders, bRows));
    tui_.tableInBox(bw, bHeaders, bRows);
    std::cout << "\n";
    int bedId = validation_.readInt("Bed id", 1, 1000000);
    Bed* bed = hospital_.findBed(bedId);
    if (!bed) throw NotFoundException("bed #" + std::to_string(bedId));
    if (bed->isFree())
        throw InvalidInputException("bed is already free");

    int patientId = bed->occupantId();
    Patient* patient = hospital_.findPatient(patientId);
    int days = bed->daysOccupied(Validator::today());
    std::string name = patient ? patient->name() : "patient #" + std::to_string(patientId);

    std::cout << "\n  About to discharge " << name
              << " from bed #" << bedId
              << " after " << days << " day(s).\n";
    if (!tui_.confirm("Confirm discharge?")) {
        tui_.toast("Cancelled.", Tui::Level::Info);
        tui_.pause();
        return;
    }

    bed->discharge();
    if (patient) patient->setBedId(-1);
    hospital_.saveAll();
    tui_.toast("Discharged.", Tui::Level::Success);
    tui_.pause();
}

void WardsModule::showAdmittedPatients() {
    tui_.clearScreen();

    std::vector<std::string> headers{ "PID", "Name", "Ward", "Bed", "Doctor", "Days" };
    std::vector<std::vector<std::string>> rows;
    std::time_t today = Validator::today();
    for (const auto& patient : hospital_.patients()) {
        if (!patient.isAdmitted()) continue;
        Bed* bed = hospital_.findBed(patient.bedId());
        Doctor* doctor = patient.hasDoctor()
            ? hospital_.findDoctor(patient.doctorId()) : nullptr;
        rows.push_back({
            std::to_string(patient.id()), patient.name(),
            bed ? bed->ward() : "-",
            "#" + std::to_string(patient.bedId()),
            doctor ? doctor->name() : "-",
            bed ? std::to_string(bed->daysOccupied(today)) : "-",
        });
    }
    int bw = tui_.bannerOpen("PATIENT CENSUS", "currently admitted",
                             {"Home", "Ward & Beds", "Census"},
                             tui_.tableBoxWidth(headers, rows));
    tui_.tableInBox(bw, headers, rows);
    std::cout << "\n";
    tui_.pause();
}


void WardsModule::run() {
    while (true) {
        char choice = tui_.menu(
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
            case '1': showAllBeds();          break;
            case '2': admitToBed();           break;
            case '3': dischargeFromBed();     break;
            case '4': showAdmittedPatients(); break;
            case 'B': return;
        }
    }
}

}
