#include <hms/Core/Exceptions.h>
#include <hms/Core/Tui.h>
#include <hms/Core/Validation.h>
#include <hms/Hospital.h>
#include <hms/Models/Appointment.h>
#include <hms/Models/Doctor.h>
#include <hms/Models/Patient.h>
#include <hms/Modules/Scheduling.h>

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace hms::scheduling {

namespace {

constexpr int CLINIC_OPEN_MIN  = 8 * 60;
constexpr int CLINIC_CLOSE_MIN = 18 * 60;

bool sameDay(std::time_t a, std::time_t b) {
    constexpr int SECS_PER_DAY = 86400;
    return (a / SECS_PER_DAY) == (b / SECS_PER_DAY);
}

bool isBusinessDay(std::time_t day) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &day);
#else
    localtime_r(&day, &tm);
#endif
    int wday = tm.tm_wday;
    return (wday >= 1 && wday <= 5);
}

std::string dayOfWeekLabel(std::time_t day) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &day);
#else
    localtime_r(&day, &tm);
#endif
    const char* labels[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };
    return labels[tm.tm_wday];
}

std::vector<Appointment*> appointmentsFor(
    Hospital& hospital, int doctorId, std::time_t day) {
    std::vector<Appointment*> matches;
    for (auto& appointment : hospital.appointments()) {
        if (appointment.doctorId() != doctorId) continue;
        if (appointment.date() != day) continue;
        if (appointment.status() == AppointmentStatus::Cancelled) continue;
        matches.push_back(&appointment);
    }
    std::sort(matches.begin(), matches.end(),
              [](Appointment* a, Appointment* b) {
                  return a->endMinutes() < b->endMinutes();
              });
    return matches;
}

bool hasConflict(const std::vector<Appointment*>& existing,
                 int startMinutes, int endMinutes) {
    for (Appointment* appointment : existing) {
        if (startMinutes < appointment->endMinutes() &&
            appointment->startMinutes() < endMinutes) {
            return true;
        }
    }
    return false;
}

std::vector<Appointment*> filterAppointments(
    const std::vector<Appointment*>& all,
    const std::string& query,
    Hospital& hospital) {
    
    std::vector<Appointment*> result;
    if (query.empty()) return all;
    
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
        [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
    
    for (auto* apt : all) {
        bool match = false;
        
        Patient* patient = hospital.findPatient(apt->patientId());
        if (patient) {
            std::string lowerName = patient->name();
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
            if (lowerName.find(lowerQuery) != std::string::npos) match = true;
        }
        
        if (!match) {
            Doctor* doctor = hospital.findDoctor(apt->doctorId());
            if (doctor) {
                std::string lowerName = doctor->name();
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                    [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
                if (lowerName.find(lowerQuery) != std::string::npos) match = true;
            }
        }
        
        if (!match) {
            std::string lowerReason = apt->reason();
            std::transform(lowerReason.begin(), lowerReason.end(), lowerReason.begin(),
                [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
            if (lowerReason.find(lowerQuery) != std::string::npos) match = true;
        }
        
        if (!match) {
            std::string dateStr = validation::formatDate(apt->date());
            std::string lowerDate = dateStr;
            std::transform(lowerDate.begin(), lowerDate.end(), lowerDate.begin(),
                [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
            if (lowerDate.find(lowerQuery) != std::string::npos) match = true;
        }
        
        if (match) result.push_back(apt);
    }
    
    return result;
}

int findNextFreeSlot(const std::vector<Appointment*>& existing,
                     int durationMinutes,
                     int afterMinutes) {
    int cursor = std::max(afterMinutes, CLINIC_OPEN_MIN);
    for (Appointment* appointment : existing) {
        if (appointment->endMinutes() <= cursor) continue;
        if (cursor + durationMinutes <= appointment->startMinutes()) {
            return cursor;
        }
        cursor = std::max(cursor, appointment->endMinutes());
    }
    if (cursor + durationMinutes <= CLINIC_CLOSE_MIN) return cursor;
    return -1;
}

void showDailySchedule(Hospital& hospital) {
    if (hospital.doctors().empty()) {
        tui::toast("No doctors in the system yet.", tui::Level::Warning);
        tui::pause();
        return;
    }
    tui::clearScreen();
    tui::banner("APPOINTMENT SCHEDULE", "", {"Home", "Appointments", "View"});
    std::cout << "\n";

    std::vector<std::string> dHeaders{"ID", "Name", "Specialty"};
    std::vector<std::vector<std::string>> dRows;
    for (const auto& doctor : hospital.doctors()) {
        dRows.push_back({
            std::to_string(doctor.id()), doctor.name(), doctor.specialty(),
        });
    }
    int dw = tui::bannerOpen("SELECT DOCTOR", "",
                             {"Home", "Appointments", "View"},
                             tui::tableBoxWidth(dHeaders, dRows));
    tui::tableInBox(dw, dHeaders, dRows);
    std::cout << "\n";
    int doctorId = validation::readInt("Doctor id", 1, 1000000);
    Doctor* doctor = hospital.findDoctor(doctorId);
    if (!doctor) throw NotFoundException("doctor #" + std::to_string(doctorId));

    std::vector<Appointment*> allAppointments;
    for (auto& appointment : hospital.appointments()) {
        if (appointment.doctorId() != doctorId) continue;
        if (appointment.status() == AppointmentStatus::Cancelled) continue;
        allAppointments.push_back(&appointment);
    }

    std::sort(allAppointments.begin(), allAppointments.end(),
              [](Appointment* a, Appointment* b) {
                  if (a->date() != b->date()) return a->date() < b->date();
                  return a->startMinutes() < b->startMinutes();
              });

    std::time_t viewDate = validation::today();
    std::string query;

    while (true) {
        tui::clearScreen();

        std::vector<Appointment*> dateFiltered;
        for (auto* apt : allAppointments) {
            if (sameDay(apt->date(), viewDate)) {
                dateFiltered.push_back(apt);
            }
        }

        auto filtered = filterAppointments(dateFiltered, query, hospital);

        std::string title = query.empty() ? "APPOINTMENTS"
            : "APPOINTMENTS MATCHING \"" + query + "\"";

        std::vector<std::string> headers{ "Apt#", "Time", "Patient", "Reason", "Status" };
        std::vector<std::vector<std::string>> rows;

        for (auto* appointment : filtered) {
            Patient* patient = hospital.findPatient(appointment->patientId());
            rows.push_back({
                "#" + std::to_string(appointment->id()),
                validation::formatTime(appointment->startMinutes() * 60) + " - " +
                    validation::formatTime(appointment->endMinutes() * 60),
                patient ? patient->name() : "(unknown)",
                appointment->reason(),
                std::string(statusColor(appointment->status())) +
                    toString(appointment->status()) + tui::color::RESET,
            });
        }

        std::string sectionTitle = "Dr. " + doctor->name() + "  •  " +
                                   validation::formatDate(viewDate) + "  •  " +
                                   std::to_string(filtered.size()) + " appointments";
        int bw = tui::bannerOpen(title, "", {"Home", "Appointments", "View"},
                                 tui::tableBoxWidth(headers, rows, sectionTitle));
        tui::tableInBox(bw, headers, rows, sectionTitle);
        std::cout << "\n";

        std::string prompt = "[Enter] Back";
        if (!query.empty()) prompt += "  [C] Clear";
        prompt += "  [type to filter]";
        std::string input = validation::readLine(prompt, true);

        if (input.empty()) return;

        if ((input == "c" || input == "C") && !query.empty()) {
            query.clear();
            viewDate = validation::today();
            continue;
        }

        try {
            std::time_t parsedDate = validation::parseDate(input);
            viewDate = parsedDate;
            query.clear();
            continue;
        } catch (...) {
            query = input;
            continue;
        }
    }
}

void bookAppointment(Hospital& hospital) {
    if (hospital.patients().empty())
        throw NotFoundException("no patients yet — register one first");
    if (hospital.doctors().empty())
        throw NotFoundException("no doctors available");

    tui::clearScreen();
    tui::banner("BOOK APPOINTMENT", "automatic conflict detection", {"Home", "Appointments", "Book"});
    std::cout << "\n";

    std::vector<std::string> pHeaders{"ID", "Name", "Age", "Sex"};
    std::vector<std::vector<std::string>> pRows;
    for (const auto& patient : hospital.patients()) {
        std::string sx = (patient.sex() == 'M' || patient.sex() == 'm') ? "Male" : "Female";
        pRows.push_back({
            std::to_string(patient.id()), patient.name(),
            std::to_string(patient.age()), sx,
        });
    }
    int pw = tui::bannerOpen("SELECT PATIENT", "",
                             {"Home", "Appointments", "Book"},
                             tui::tableBoxWidth(pHeaders, pRows));
    tui::tableInBox(pw, pHeaders, pRows);
    std::cout << "\n";
    int patientId = validation::readInt("Patient id", 1, 1000000);
    Patient* patient = hospital.findPatient(patientId);
    if (!patient) throw NotFoundException("patient #" + std::to_string(patientId));

    std::vector<std::string> dHeaders{"ID", "Name", "Specialty", "Fee"};
    std::vector<std::vector<std::string>> dRows;
    for (const auto& doctor : hospital.doctors()) {
        std::ostringstream feeStr;
        feeStr << "P" << std::fixed << std::setprecision(2) << doctor.consultFee();
        dRows.push_back({
            std::to_string(doctor.id()), doctor.name(),
            doctor.specialty(), feeStr.str(),
        });
    }
    int dw = tui::bannerOpen("SELECT DOCTOR", "",
                             {"Home", "Appointments", "Book"},
                             tui::tableBoxWidth(dHeaders, dRows));
    tui::tableInBox(dw, dHeaders, dRows);
    std::cout << "\n";
    int doctorId = validation::readInt("Doctor id", 1, 1000000);
    Doctor* doctor = hospital.findDoctor(doctorId);
    if (!doctor) throw NotFoundException("doctor #" + std::to_string(doctorId));

    std::time_t day = validation::readDate("Appointment date");

    if (!isBusinessDay(day)) {
        throw InvalidInputException(
            "clinic is closed on " + dayOfWeekLabel(day) +
            ". Business days are Monday through Friday.");
    }

    int startMinutesOfDay = validation::readTimeOfDay("Start time") / 60;
    int endMinutesOfDay = validation::readTimeOfDay("End time") / 60;
    int duration = endMinutesOfDay - startMinutesOfDay;

    if (startMinutesOfDay < CLINIC_OPEN_MIN || endMinutesOfDay > CLINIC_CLOSE_MIN) {
        throw InvalidInputException(
            "clinic hours are " +
            validation::formatTime(CLINIC_OPEN_MIN * 60) + " to " +
            validation::formatTime(CLINIC_CLOSE_MIN * 60));
    }

    if (duration <= 0) {
        throw InvalidInputException("end time must be after start time");
    }

    if (duration < 5 || duration > 240) {
        throw InvalidInputException("appointment must be between 5 and 240 minutes");
    }

    auto existing = appointmentsFor(hospital, doctorId, day);
    if (static_cast<int>(existing.size()) >= doctor->dailyAppointmentLimit()) {
        throw CapacityException(
            "Dr. " + doctor->name() + " already has " +
            std::to_string(existing.size()) +
            " appointments that day (limit " +
            std::to_string(doctor->dailyAppointmentLimit()) + ")");
    }

    if (hasConflict(existing, startMinutesOfDay, endMinutesOfDay)) {
        int suggestedStart = findNextFreeSlot(existing, duration, startMinutesOfDay);
        std::ostringstream message;
        message << "Time conflicts with another appointment.";
        if (suggestedStart >= 0) {
            message << " Next free slot: "
                    << validation::formatTime(suggestedStart * 60)
                    << " - "
                    << validation::formatTime((suggestedStart + duration) * 60);
        } else {
            message << " No free slot remaining that day.";
        }
        tui::toast(message.str(), tui::Level::Warning);
        if (suggestedStart >= 0 && tui::confirm("Use the suggested time?")) {
            startMinutesOfDay = suggestedStart;
            endMinutesOfDay = suggestedStart + duration;
        } else {
            tui::pause();
            return;
        }
    }

    std::string reason = validation::readLine("Reason for visit", true);

    Appointment appointment(
        hospital.nextAppointmentId(), patientId, doctorId, day,
        startMinutesOfDay, endMinutesOfDay, reason);
    hospital.appointments().push_back(appointment);
    hospital.saveAll();

    tui::toast(
        "Booked: " + patient->name() + " with Dr. " + doctor->name() +
        " on " + validation::formatDate(day) + " " +
        validation::formatTime(startMinutesOfDay * 60),
        tui::Level::Success);
    tui::pause();
}

void cancelAppointment(Hospital& hospital) {
    tui::clearScreen();
    tui::banner("CANCEL APPOINTMENT", "", {"Home", "Appointments", "Cancel"});
    std::cout << "\n";

    std::vector<std::string> aHeaders{"Apt#", "Patient", "Doctor", "Date", "Time", "Status"};
    std::vector<std::vector<std::string>> aRows;
    for (const auto& apt : hospital.appointments()) {
        if (apt.status() == AppointmentStatus::Cancelled) continue;
        Patient* p = hospital.findPatient(apt.patientId());
        Doctor* d = hospital.findDoctor(apt.doctorId());
        aRows.push_back({
            "#" + std::to_string(apt.id()),
            p ? p->name() : "?",
            d ? std::string("Dr. ") + d->name() : "?",
            validation::formatDate(apt.date()),
            validation::formatTime(apt.startMinutes() * 60),
            toString(apt.status()),
        });
    }
    int aw = tui::bannerOpen("SELECT APPOINTMENT", "",
                             {"Home", "Appointments", "Cancel"},
                             tui::tableBoxWidth(aHeaders, aRows));
    tui::tableInBox(aw, aHeaders, aRows);
    std::cout << "\n";
    int id = validation::readInt("Appointment id", 1, 1000000);
    for (auto& appointment : hospital.appointments()) {
        if (appointment.id() == id) {
            if (appointment.status() == AppointmentStatus::Cancelled) {
                tui::toast("Already cancelled.", tui::Level::Info);
            } else {
                if (!tui::confirm("Cancel this appointment?")) {
                    tui::toast("Cancelled.", tui::Level::Info);
                    tui::pause();
                    return;
                }
                appointment.setStatus(AppointmentStatus::Cancelled);
                hospital.saveAll();
                tui::toast("Cancelled.", tui::Level::Success);
            }
            tui::pause();
            return;
        }
    }
    throw NotFoundException("appointment #" + std::to_string(id));
}

void markCompleted(Hospital& hospital) {
    tui::clearScreen();
    tui::banner("MARK APPOINTMENT COMPLETED", "", {"Home", "Appointments", "Complete"});
    std::cout << "\n";

    std::vector<std::string> aHeaders{"Apt#", "Patient", "Doctor", "Date", "Time", "Status"};
    std::vector<std::vector<std::string>> aRows;
    for (const auto& apt : hospital.appointments()) {
        if (apt.status() != AppointmentStatus::Scheduled) continue;
        Patient* p = hospital.findPatient(apt.patientId());
        Doctor* d = hospital.findDoctor(apt.doctorId());
        aRows.push_back({
            "#" + std::to_string(apt.id()),
            p ? p->name() : "?",
            d ? std::string("Dr. ") + d->name() : "?",
            validation::formatDate(apt.date()),
            validation::formatTime(apt.startMinutes() * 60),
            toString(apt.status()),
        });
    }
    int aw = tui::bannerOpen("SELECT APPOINTMENT", "",
                             {"Home", "Appointments", "Complete"},
                             tui::tableBoxWidth(aHeaders, aRows));
    tui::tableInBox(aw, aHeaders, aRows);
    std::cout << "\n";
    int id = validation::readInt("Appointment id", 1, 1000000);
    for (auto& appointment : hospital.appointments()) {
        if (appointment.id() == id) {
            appointment.setStatus(AppointmentStatus::Completed);
            hospital.saveAll();
            tui::toast("Marked completed.", tui::Level::Success);
            tui::pause();
            return;
        }
    }
    throw NotFoundException("appointment #" + std::to_string(id));
}

void suggestNextFreeSlot(Hospital& hospital) {
    if (hospital.doctors().empty()) {
        tui::toast("No doctors in the system yet.", tui::Level::Warning);
        tui::pause();
        return;
    }
    tui::clearScreen();
    tui::banner("FIND NEXT FREE SLOT", "", {"Home", "Appointments", "Find Slot"});
    std::cout << "\n";

    std::vector<std::string> dHeaders{"ID", "Name", "Specialty"};
    std::vector<std::vector<std::string>> dRows;
    for (const auto& doctor : hospital.doctors()) {
        dRows.push_back({
            std::to_string(doctor.id()), doctor.name(), doctor.specialty(),
        });
    }
    int dw = tui::bannerOpen("SELECT DOCTOR", "",
                             {"Home", "Appointments", "Find Slot"},
                             tui::tableBoxWidth(dHeaders, dRows));
    tui::tableInBox(dw, dHeaders, dRows);
    std::cout << "\n";
    int doctorId = validation::readInt("Doctor id", 1, 1000000);
    Doctor* doctor = hospital.findDoctor(doctorId);
    if (!doctor) throw NotFoundException("doctor #" + std::to_string(doctorId));

    int duration = validation::readDuration("Duration", 5, 240);

    std::time_t searchDate = validation::today();
    int businessDaysSearched = 0;
    const int maxBusinessDaysToSearch = 30;

    while (businessDaysSearched < maxBusinessDaysToSearch) {
        if (isBusinessDay(searchDate)) {
            auto existing = appointmentsFor(hospital, doctorId, searchDate);

            if (static_cast<int>(existing.size()) < doctor->dailyAppointmentLimit()) {
                int suggested = findNextFreeSlot(existing, duration, CLINIC_OPEN_MIN);
                if (suggested >= 0) {
                    std::ostringstream message;
                    message << "Next free slot: "
                            << validation::formatDate(searchDate) << " "
                            << validation::formatTime(suggested * 60) << " - "
                            << validation::formatTime((suggested + duration) * 60);
                    tui::toast(message.str(), tui::Level::Success);
                    tui::pause();
                    return;
                }
            }
            ++businessDaysSearched;
        }

        searchDate += 86400;
    }

    tui::toast("No free slot found in the next " + std::to_string(maxBusinessDaysToSearch) + " business days.",
               tui::Level::Warning);
    tui::pause();
}

}

void run(Hospital& hospital) {
    while (true) {
        char choice = tui::menu(
            "APPOINTMENTS",
            {"Home", "Appointments"},
            {
                { '1', "View daily schedule",   "see one doctor's day" },
                { '2', "Book appointment",      "with conflict detection" },
                { '3', "Find next free slot",   "earliest available date and time" },
                { '4', "Cancel appointment",    "set status to cancelled" },
                { '5', "Mark completed",        "set status to completed" },
                { 'B', "Back",                  "return to main menu" },
            });
        switch (choice) {
            case '1': showDailySchedule(hospital);    break;
            case '2': bookAppointment(hospital);      break;
            case '3': suggestNextFreeSlot(hospital);  break;
            case '4': cancelAppointment(hospital);    break;
            case '5': markCompleted(hospital);        break;
            case 'B': return;
        }
    }
}

}
