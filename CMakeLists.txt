cmake_minimum_required(VERSION 3.14)
project(hospital-management-system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hospital
    src/main.cpp
    src/Application.cpp
    src/Hospital.cpp
    src/Core/Tui.cpp
    src/Core/Validation.cpp
    src/Models/Patient.cpp
    src/Models/Doctor.cpp
    src/Models/Bed.cpp
    src/Models/Medicine.cpp
    src/Models/Appointment.cpp
    src/Models/Bill.cpp
    src/Modules/Triage.cpp
    src/Modules/Scheduling.cpp
    src/Modules/Wards.cpp
    src/Modules/Pharmacy.cpp
    src/Modules/Dispatch.cpp
    src/Modules/Billing.cpp
    src/Modules/Reports.cpp
    src/Modules/Patients.cpp
    src/Modules/Doctors.cpp
)

target_include_directories(hospital PRIVATE include)
target_compile_options(hospital PRIVATE -Wall -Wextra)

set_target_properties(hospital PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
