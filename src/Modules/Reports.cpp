#include <hms/Core/Exceptions.h>
#include <hms/Core/Format.h>
#include <hms/Core/Tui.h>
#include <hms/Core/Validation.h>
#include <hms/Hospital.h>
#include <hms/Models/Appointment.h>
#include <hms/Models/Bed.h>
#include <hms/Models/Bill.h>
#include <hms/Models/Doctor.h>
#include <hms/Models/Medicine.h>
#include <hms/Models/Patient.h>
#include <hms/Modules/Reports.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

namespace hms::reports {

namespace {

std::string percent(double ratio) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << (ratio * 100.0) << "%";
    return out.str();
}

bool sameDay(std::time_t a, std::time_t b) {
    constexpr int SECS_PER_DAY = 86400;
    return (a / SECS_PER_DAY) == (b / SECS_PER_DAY);
}

void dailySummary(const Hospital& hospital) {
    tui::clearScreen();
    tui::banner("DAILY SUMMARY", "snapshot across the whole hospital", {"Home", "Reports", "Daily Summary"});

    std::time_t today = validation::today();

    int patientsToday = 0;
    for (const auto& patient : hospital.patients()) {
        if (sameDay(patient.arrival(), today)) ++patientsToday;
    }
    int erWaiting = 0;
    for (const auto& patient : hospital.patients()) {
        if (!patient.isAdmitted() && !patient.hasDoctor()) ++erWaiting;
    }
    int bedsOccupied = 0;
    for (const auto& bed : hospital.beds()) {
        if (!bed.isFree()) ++bedsOccupied;
    }
    int lowStock = 0;
    int nearExpiry = 0;
    for (const auto& medicine : hospital.medicines()) {
        if (medicine.isLowStock()) ++lowStock;
        if (medicine.daysUntilExpiry(today) <= 30) ++nearExpiry;
    }
    double revenueToday = 0.0;
    int billsToday = 0;
    for (const auto& bill : hospital.bills()) {
        if (sameDay(bill.date(), today)) {
            revenueToday += bill.total();
            ++billsToday;
        }
    }
    int appointmentsToday = 0;
    for (const auto& appointment : hospital.appointments()) {
        if (appointment.date() == today &&
            appointment.status() != AppointmentStatus::Cancelled) {
            ++appointmentsToday;
        }
    }

    auto bedCount = hospital.beds().size();
    double occupancyRatio = bedCount
        ? static_cast<double>(bedsOccupied) / bedCount : 0.0;

    std::vector<std::string> headers{"Metric", "Value"};
    std::vector<std::vector<std::string>> rows{
        {"Date", validation::formatDate(today)},
        {"Patients registered today", std::to_string(patientsToday)},
        {"ER queue waiting", std::to_string(erWaiting)},
        {"Appointments today", std::to_string(appointmentsToday)},
        {"Beds occupied", std::to_string(bedsOccupied) + "/" + std::to_string(bedCount)},
        {"Bed occupancy", percent(occupancyRatio)},
        {"Low-stock medicines", std::to_string(lowStock)},
        {"Near-expiry medicines", std::to_string(nearExpiry)},
        {"Bills issued today", std::to_string(billsToday)},
        {"Revenue today", format::money(revenueToday)},
    };
    int bw = tui::bannerOpen("DAILY SUMMARY", "", {"Home", "Reports", "Daily Summary"},
                             tui::tableBoxWidth(headers, rows));
    tui::tableInBox(bw, headers, rows);
    std::cout << "\n";
    tui::pause();
}

void bedOccupancy(const Hospital& hospital) {
    tui::clearScreen();
    tui::banner("BED OCCUPANCY", "by ward", {"Home", "Reports", "Bed Occupancy"});
    std::cout << "\n";

    std::map<std::string, std::pair<int, int>> wardStats;
    for (const auto& bed : hospital.beds()) {
        auto& entry = wardStats[bed.ward()];
        entry.second += 1;
        if (!bed.isFree()) entry.first += 1;
    }
    std::vector<std::string> headers{"Ward", "Occupied", "Total", "Percentage"};
    std::vector<std::vector<std::string>> rows;
    for (const auto& [ward, stats] : wardStats) {
        double ratio = stats.second ? static_cast<double>(stats.first) /
                                       stats.second : 0.0;
        rows.push_back({
            ward,
            std::to_string(stats.first),
            std::to_string(stats.second),
            percent(ratio),
        });
    }
    int bw = tui::bannerOpen("BED OCCUPANCY BY WARD", "",
                             {"Home", "Reports", "Bed Occupancy"},
                             tui::tableBoxWidth(headers, rows));
    tui::tableInBox(bw, headers, rows);
    std::cout << "\n";
    tui::pause();
}

void doctorWorkload(const Hospital& hospital) {
    tui::clearScreen();

    std::vector<std::string> headers{
        "ID", "Name", "Specialty", "Today's apts", "Patients", "Daily limit", "Utilisation" };
    std::time_t today = validation::today();
    std::vector<std::vector<std::string>> rows;
    for (const auto& doctor : hospital.doctors()) {
        int appointments = 0;
        for (const auto& appointment : hospital.appointments()) {
            if (appointment.doctorId() != doctor.id()) continue;
            if (appointment.date() != today) continue;
            if (appointment.status() == AppointmentStatus::Cancelled) continue;
            ++appointments;
        }
        int assignedPatients = 0;
        for (const auto& patient : hospital.patients()) {
            if (patient.doctorId() == doctor.id()) ++assignedPatients;
        }
        int totalLoad = appointments + assignedPatients;
        double utilisation = doctor.dailyAppointmentLimit()
            ? static_cast<double>(totalLoad) / doctor.dailyAppointmentLimit() : 0.0;
        rows.push_back({
            std::to_string(doctor.id()), doctor.name(), doctor.specialty(),
            std::to_string(appointments), std::to_string(assignedPatients),
            std::to_string(doctor.dailyAppointmentLimit()), percent(utilisation),
        });
    }
    int bw = tui::bannerOpen("DOCTOR WORKLOAD", "per doctor today",
                             {"Home", "Reports", "Doctor Workload"},
                             tui::tableBoxWidth(headers, rows));
    tui::tableInBox(bw, headers, rows);
    std::cout << "\n";
    tui::pause();
}

void revenueAndDiscounts(const Hospital& hospital) {
    tui::clearScreen();
    tui::banner("REVENUE & DISCOUNTS", "", {"Home", "Reports", "Revenue"});

    double gross = 0.0;
    double discounted = 0.0;
    double net = 0.0;
    std::unordered_map<std::string, double> discountByCode;

    for (const auto& bill : hospital.bills()) {
        gross      += bill.subtotal();
        discounted += bill.discountAmount();
        net        += bill.total();
        if (!bill.discountApplied().empty()) {
            discountByCode[bill.discountApplied()] += bill.discountAmount();
        }
    }

    std::vector<std::string> fHeaders{"Metric", "Amount"};
    std::vector<std::vector<std::string>> fRows{
        {"Gross (subtotals)", format::money(gross)},
        {"Total discounts", "-" + format::money(discounted)},
        {"Net (collected)", format::money(net)},
        {"Bills count", std::to_string(hospital.bills().size())},
    };
    int fw = tui::bannerOpen("REVENUE SUMMARY", "", {"Home", "Reports", "Revenue"},
                             tui::tableBoxWidth(fHeaders, fRows));
    tui::tableInBox(fw, fHeaders, fRows);
    std::cout << "\n";

    std::vector<std::vector<std::string>> rows;
    for (const auto& [code, amount] : discountByCode) {
        rows.push_back({ code, format::money(amount) });
    }
    tui::table({ "Discount combination", "Amount" }, rows,
               "By discount code");
    std::cout << "\n";
    tui::pause();
}

void exportAll(const Hospital& hospital) {
    std::time_t today = validation::today();
    std::string dateLabel = validation::formatDate(today);
    std::string base = hospital.dataDir() + "/reports/" + dateLabel;

    auto ensureDir = [](const std::string& path) {
        struct stat st;
        if (::stat(path.c_str(), &st) != 0) ::mkdir(path.c_str(), 0755);
    };
    ensureDir(hospital.dataDir() + "/reports");
    ensureDir(base);

    {
        std::ofstream out(base + "/census.csv");
        out << "patient_id,name,ward,bed,doctor_id,days\n";
        std::time_t day = validation::today();
        for (const auto& patient : hospital.patients()) {
            if (!patient.isAdmitted()) continue;
            const Bed* assignedBed = nullptr;
            for (const auto& bed : hospital.beds())
                if (bed.id() == patient.bedId()) { assignedBed = &bed; break; }
            out << patient.id() << "," << patient.name() << ","
                << (assignedBed ? assignedBed->ward() : "-") << ","
                << patient.bedId() << ","
                << patient.doctorId() << ","
                << (assignedBed ? assignedBed->daysOccupied(day) : 0)
                << "\n";
        }
    }
    {
        std::ofstream out(base + "/revenue.csv");
        out << "bill_id,date,patient_id,subtotal,discount,total\n";
        for (const auto& bill : hospital.bills()) {
            out << bill.id() << ","
                << validation::formatDate(bill.date()) << ","
                << bill.patientId() << ","
                << bill.subtotal() << ","
                << bill.discountAmount() << ","
                << bill.total() << "\n";
        }
    }
    {
        std::ofstream out(base + "/pharmacy.csv");
        out << "sku,name,stock,reorder,expiry,days_left\n";
        for (const auto& medicine : hospital.medicines()) {
            out << medicine.sku() << "," << medicine.name() << ","
                << medicine.stock() << "," << medicine.reorderLevel() << ","
                << validation::formatDate(medicine.expiry()) << ","
                << medicine.daysUntilExpiry(today) << "\n";
        }
    }
    tui::toast("Exported daily reports to " + base, tui::Level::Success);
    tui::pause();
}

}

void run(Hospital& hospital) {
    while (true) {
        char choice = tui::menu(
            "REPORTS",
            {"Home", "Reports"},
            {
                { '1', "Daily summary",       "one-screen hospital snapshot" },
                { '2', "Bed occupancy",       "by ward, with bar chart" },
                { '3', "Revenue & discounts", "totals across all bills" },
                { '4', "Doctor workload",     "today's load per doctor" },
                { '5', "Export all to CSV",   "into data/reports/<date>/" },
                { 'B', "Back",                "return to main menu" },
            });
        switch (choice) {
            case '1': dailySummary(hospital);        break;
            case '2': bedOccupancy(hospital);        break;
            case '3': revenueAndDiscounts(hospital); break;
            case '4': doctorWorkload(hospital);      break;
            case '5': exportAll(hospital);           break;
            case 'B': return;
        }
    }
}

}
