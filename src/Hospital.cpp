#include <hms/Core/Repository.h>
#include <hms/Core/Validation.h>
#include <hms/Hospital.h>

#include <algorithm>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>

namespace hms {

static void ensureDir(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) {
        ::mkdir(path.c_str(), 0755);
    }
}

void Hospital::loadAll() {
    ensureDir(dataDir_);

    patients_     = Repository<Patient>::loadAll(patientsPath());
    doctors_      = Repository<Doctor>::loadAll(doctorsPath());
    appointments_ = Repository<Appointment>::loadAll(appointmentsPath());
    medicines_    = Repository<Medicine>::loadAll(medicinesPath());
    beds_         = Repository<Bed>::loadAll(bedsPath());
    bills_        = Repository<Bill>::loadAll(billsPath());

    rebuildIdCounters();
    seedIfEmpty();
}

void Hospital::saveAll() {
    Repository<Patient>::saveAll(patientsPath(), patients_);
    Repository<Doctor>::saveAll(doctorsPath(), doctors_);
    Repository<Appointment>::saveAll(appointmentsPath(), appointments_);
    Repository<Medicine>::saveAll(medicinesPath(), medicines_);
    Repository<Bed>::saveAll(bedsPath(), beds_);
    Repository<Bill>::saveAll(billsPath(), bills_);
}

void Hospital::rebuildIdCounters() {
    auto maxId = [](auto& container, auto getter) {
        int max = 0;
        for (const auto& item : container)
            max = std::max(max, getter(item));
        return max + 1;
    };

    nextPatientId_     = maxId(patients_,     [](const Patient& p)     { return p.id(); });
    nextDoctorId_      = maxId(doctors_,      [](const Doctor& d)      { return d.id(); });
    nextAppointmentId_ = maxId(appointments_, [](const Appointment& a) { return a.id(); });
    nextBedId_         = maxId(beds_,         [](const Bed& b)         { return b.id(); });
    nextBillId_        = maxId(bills_,        [](const Bill& b)        { return b.id(); });
}

void Hospital::seedIfEmpty() {
    bool seeded = false;
    if (doctors_.empty())   { seedDoctors();   seeded = true; }
    if (beds_.empty())      { seedBeds();      seeded = true; }
    if (medicines_.empty()) { seedMedicines(); seeded = true; }

    if (!fileExists(graphPath())) {
        seedZoneGraph();
    }

    if (seeded) {
        rebuildIdCounters();
        saveAll();
    }
}

void Hospital::seedDoctors() {
    doctors_.push_back(Doctor(nextDoctorId_++, "Maria Santos", 42, 'F',
        "0917-100-0001", "Legazpi", "General Medicine", "G-101", 600.0, 12));
    doctors_.push_back(Doctor(nextDoctorId_++, "Jose Reyes", 51, 'M',
        "0917-100-0002", "Daraga", "Cardiology", "C-202", 1200.0, 8));
    doctors_.push_back(Doctor(nextDoctorId_++, "Anna Cruz", 36, 'F',
        "0917-100-0003", "Tabaco", "Pediatrics", "P-301", 700.0, 10));
    doctors_.push_back(Doctor(nextDoctorId_++, "Mark Lim", 47, 'M',
        "0917-100-0004", "Sorsogon", "Surgery", "S-401", 1500.0, 6));
    doctors_.push_back(Doctor(nextDoctorId_++, "Liza Gomez", 33, 'F',
        "0917-100-0005", "Legazpi", "Obstetrics", "O-501", 900.0, 8));
}

void Hospital::seedBeds() {
    auto addBeds = [&](const std::string& ward, int count, double rate) {
        for (int i = 0; i < count; ++i) {
            beds_.push_back(Bed(nextBedId_++, ward, rate));
        }
    };
    addBeds("General", 6, 800.0);
    addBeds("Private", 4, 1800.0);
    addBeds("ICU",     2, 5000.0);
    addBeds("ER",      4, 1200.0);
}

void Hospital::seedMedicines() {
    std::time_t day = validation::today();
    auto inDays = [&](int days) { return day + days * 86400LL; };

    medicines_.push_back(Medicine("PAR500", "Paracetamol 500mg",  120, 30,  2.50,  inDays(180)));
    medicines_.push_back(Medicine("AMO500", "Amoxicillin 500mg",   60, 20,  6.75,  inDays(20)));
    medicines_.push_back(Medicine("IBU400", "Ibuprofen 400mg",     80, 25,  3.10,  inDays(365)));
    medicines_.push_back(Medicine("CET010", "Cetirizine 10mg",     45, 15,  4.40,  inDays(75)));
    medicines_.push_back(Medicine("OMP020", "Omeprazole 20mg",     12, 20, 12.00,  inDays(10)));
    medicines_.push_back(Medicine("MET500", "Metformin 500mg",     90, 30,  3.80,  inDays(220)));
    medicines_.push_back(Medicine("SAL100", "Salbutamol inhaler",   8, 10, 180.00, inDays(5)));
    medicines_.push_back(Medicine("LOS050", "Losartan 50mg",       55, 20,  8.20,  inDays(150)));
}

void Hospital::seedZoneGraph() {
    std::ofstream out(graphPath());
    if (!out) return;
    out << "# zone graph used by ambulance dispatch\n"
        << "# format: NODES nodeName,nodeName,... (first node = ambulance station)\n"
        << "# format: EDGE fromNode toNode minutes\n"
        << "NODES Station,Downtown,Riverside,Hillside,Northgate,Eastpark,SouthBay,Airport\n"
        << "EDGE Station Downtown 4\n"
        << "EDGE Station Northgate 6\n"
        << "EDGE Station Riverside 7\n"
        << "EDGE Downtown Hillside 5\n"
        << "EDGE Downtown Eastpark 3\n"
        << "EDGE Downtown SouthBay 9\n"
        << "EDGE Riverside Hillside 4\n"
        << "EDGE Hillside Northgate 6\n"
        << "EDGE Eastpark SouthBay 5\n"
        << "EDGE Eastpark Airport 8\n"
        << "EDGE SouthBay Airport 6\n"
        << "EDGE Northgate Airport 12\n";
}

Patient* Hospital::findPatient(int id) {
    for (auto& p : patients_) if (p.id() == id) return &p;
    return nullptr;
}

Doctor* Hospital::findDoctor(int id) {
    for (auto& d : doctors_) if (d.id() == id) return &d;
    return nullptr;
}

Bed* Hospital::findBed(int id) {
    for (auto& b : beds_) if (b.id() == id) return &b;
    return nullptr;
}

Medicine* Hospital::findMedicine(const std::string& sku) {
    for (auto& m : medicines_) if (m.sku() == sku) return &m;
    return nullptr;
}

}
