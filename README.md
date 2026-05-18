# Hospital Management System

A terminal-based Hospital Management System (HMS) written in C++17. Provides a full-featured TUI for managing patients, doctors, appointments, pharmacy inventory, ward beds, billing, and ambulance dispatch.

## Features

### Emergency Room (Triage)
- Priority queue ordered by severity then arrival time
- Admit new patients with severity assessment (1-5 scale)
- Call next patient with doctor assignment
- Update severity levels dynamically

### Appointments & Scheduling
- Book appointments with automatic conflict detection
- View daily schedule with date filtering and text search
- Find next free slot across business days using gap analysis
- Cancel and mark appointments as completed
- Business hours: Mon-Fri, 8:00 AM - 6:00 PM

### Patient Records
- Register, edit, delete, and search patients
- Tracks senior citizen, PWD, and PhilHealth flags
- Promo code support for billing discounts
- Referential integrity: blocks deletion if active appointments or bills exist

### Doctor Records
- Register, edit, delete, and search doctors
- Consultation fee and daily appointment limits
- **Auto-assign doctors** using the Hungarian algorithm for balanced workload distribution
- Referential integrity: cancels appointments and clears patient assignments on delete

### Ward & Beds
- View bed occupancy across wards (General, Private, ICU, ER)
- Admit and discharge patients
- Patient census of all currently admitted patients

### Pharmacy
- Add, edit, delete medicines with SKU-based inventory
- Restock and dispense with stock validation
- Low stock alerts (below reorder threshold)
- Near-expiry alerts (within 30 days)
- Expiry validation on add and edit

### Billing
- Create bills with automatic room charges and doctor fees
- Manual procedure/item entry
- **Optimal discount calculation** using dynamic programming (0/1 knapsack with group constraints)
- Bill status tracking: Unpaid, Paid, Voided
- Duplicate bill prevention (one unpaid bill per patient)
- View and update bill status

### Ambulance Dispatch
- Zone graph with weighted edges
- **Dijkstra's algorithm** for fastest route calculation
- Request ambulance with ETA and path visualization
- View all zone distances from station

### Reports
- Daily summary: patients, ER queue, beds, pharmacy, revenue
- Bed occupancy by ward
- Doctor workload and utilization
- Revenue and discount analytics
- Export to CSV

## Architecture

```
include/hms/
  Core/
    Csv.h          - CSV parsing and escaping utilities
    Exceptions.h   - Exception hierarchy (HospitalException, NotFoundException, etc.)
    Format.h       - Shared format::money() utility
    Repository.h   - Generic template for CSV-based data persistence
    Tui.h          - Terminal UI components (menus, tables, banners, toasts)
    Validation.h   - Input validation and reading functions
  Models/
    Patient.h      - Patient entity with severity, flags, admission state
    Doctor.h       - Doctor entity with specialty, fees, limits
    Appointment.h  - Appointment with time slots and status
    Bed.h          - Bed with ward, occupancy, daily rate
    Medicine.h     - Medicine with stock, expiry, reorder level
    Bill.h         - Bill with items, discounts, status
  Hospital.h       - Central facade managing all data and ID counters
  Modules/
    Triage.h       - ER queue management
    Scheduling.h   - Appointment booking and calendar
    Patients.h     - Patient CRUD operations
    Doctors.h      - Doctor CRUD + auto-assignment
    Wards.h        - Bed management
    Pharmacy.h     - Medicine inventory
    Billing.h      - Invoice generation
    Dispatch.h     - Ambulance routing
    Reports.h      - Analytics and export

src/
  main.cpp         - Entry point, signal handling, main menu loop
  Hospital.cpp     - Data loading, saving, seeding
  Core/            - TUI rendering, validation logic
  Models/          - CSV serialization for each entity
  Modules/         - Interactive flows for each subsystem
```

## Algorithms Used

| Algorithm | Location | Purpose |
|-----------|----------|---------|
| **Hungarian Algorithm** | `Doctors.cpp` | Optimal patient-doctor assignment balancing severity and workload |
| **Dijkstra's Algorithm** | `Dispatch.cpp` | Shortest path for ambulance routing |
| **Dynamic Programming** | `Billing.cpp` | Optimal discount combination (0/1 knapsack with group constraints) |
| **Priority Queue** | `Triage.cpp`, `Pharmacy.cpp` | ER triage ordering, medicine expiry sorting |
| **Gap Analysis** | `Scheduling.cpp` | Finding next free appointment slot |

## Building

### Requirements
- C++17 compiler (GCC 8+, Clang 7+, MSVC 2017+)
- CMake 3.14+
- Linux/macOS/Windows (terminal with ANSI escape support)

### Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Or use the provided script:

```bash
./build.sh
```

### Run

```bash
./build/hospital              # uses ./data as data directory
./build/hospital my_data/     # uses custom data directory
```

## Data Storage

All data is stored as pipe-delimited CSV files in the `data/` directory:

| File | Contents |
|------|----------|
| `patients.csv` | Patient records with demographics and flags |
| `doctors.csv` | Doctor records with specialty and fees |
| `appointments.csv` | Scheduled appointments with time slots |
| `beds.csv` | Bed inventory with occupancy state |
| `medicines.csv` | Pharmacy inventory with expiry dates |
| `bills.csv` | Billing records with items and discounts |
| `graph.csv` | Zone graph for ambulance dispatch |
| `audit.log` | Audit trail of all CRUD operations |

Data is saved atomically (write to temp file, then rename) to prevent corruption. CSV parsing skips malformed lines with a warning instead of crashing.

## Safety Features

- **Referential integrity**: Cannot delete patients/doctors with active appointments or bills
- **Input validation**: All user input validated with type, range, and format checks
- **Duplicate prevention**: Contact number uniqueness, single unpaid bill per patient
- **Audit trail**: All create/update/delete operations logged with timestamps
- **Signal handling**: Data saved on SIGINT/SIGTERM
- **Corruption recovery**: Malformed CSV lines skipped gracefully
- **Use-after-free prevention**: Delete functions save data to locals before vector erase

## License

MIT
