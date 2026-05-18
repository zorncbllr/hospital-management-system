#ifndef HMS_HOSPITAL_H
#define HMS_HOSPITAL_H

#include <hms/Models/Appointment.h>
#include <hms/Models/Bed.h>
#include <hms/Models/Bill.h>
#include <hms/Models/Doctor.h>
#include <hms/Models/Medicine.h>
#include <hms/Models/Patient.h>

#include <string>
#include <vector>

namespace hms {

class Hospital {
    std::string dataDir_;

    std::vector<Patient>     patients_;
    std::vector<Doctor>      doctors_;
    std::vector<Appointment> appointments_;
    std::vector<Medicine>    medicines_;
    std::vector<Bed>         beds_;
    std::vector<Bill>        bills_;

    int nextPatientId_     = 1;
    int nextDoctorId_      = 1;
    int nextAppointmentId_ = 1;
    int nextBedId_         = 1;
    int nextBillId_        = 1;

public:
    explicit Hospital(std::string dataDir) : dataDir_(std::move(dataDir)) {}

    const std::string& dataDir() const { return dataDir_; }

    void loadAll();
    void saveAll();
    void seedIfEmpty();

    std::vector<Patient>&     patients()     { return patients_; }
    std::vector<Doctor>&      doctors()      { return doctors_; }
    std::vector<Appointment>& appointments() { return appointments_; }
    std::vector<Medicine>&    medicines()    { return medicines_; }
    std::vector<Bed>&         beds()         { return beds_; }
    std::vector<Bill>&        bills()        { return bills_; }

    const std::vector<Patient>&     patients()     const { return patients_; }
    const std::vector<Doctor>&      doctors()      const { return doctors_; }
    const std::vector<Appointment>& appointments() const { return appointments_; }
    const std::vector<Medicine>&    medicines()    const { return medicines_; }
    const std::vector<Bed>&         beds()         const { return beds_; }
    const std::vector<Bill>&        bills()        const { return bills_; }

    Patient*   findPatient(int id);
    Doctor*    findDoctor(int id);
    Bed*       findBed(int id);
    Medicine*  findMedicine(const std::string& sku);

    int nextPatientId()     { return nextPatientId_++; }
    int nextDoctorId()      { return nextDoctorId_++; }
    int nextAppointmentId() { return nextAppointmentId_++; }
    int nextBedId()         { return nextBedId_++; }
    int nextBillId()        { return nextBillId_++; }

    std::string patientsPath()     const { return dataDir_ + "/patients.csv"; }
    std::string doctorsPath()      const { return dataDir_ + "/doctors.csv"; }
    std::string appointmentsPath() const { return dataDir_ + "/appointments.csv"; }
    std::string medicinesPath()    const { return dataDir_ + "/medicines.csv"; }
    std::string bedsPath()         const { return dataDir_ + "/beds.csv"; }
    std::string billsPath()        const { return dataDir_ + "/bills.csv"; }
    std::string graphPath()        const { return dataDir_ + "/graph.csv"; }

private:
    void rebuildIdCounters();
    void seedDoctors();
    void seedBeds();
    void seedMedicines();
    void seedZoneGraph();
};

}

#endif
