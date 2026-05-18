#include <hms/Core/Exceptions.h>
#include <hms/Hospital.h>
#include <hms/Modules/Billing.h>
#include <hms/Modules/Dispatch.h>
#include <hms/Modules/Doctors.h>
#include <hms/Modules/Patients.h>
#include <hms/Modules/Pharmacy.h>
#include <hms/Modules/Reports.h>
#include <hms/Modules/Scheduling.h>
#include <hms/Modules/Triage.h>
#include <hms/Core/Tui.h>
#include <hms/Modules/Wards.h>

#include <csignal>
#include <iostream>
#include <string>

using namespace hms;

static Hospital *g_hospital = nullptr;

static void onSignal(int)
{
    if (g_hospital)
    {
        try
        {
            g_hospital->saveAll();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Warning: failed to save data on exit: " << e.what() << "\n";
        }
    }
    std::exit(0);
}

static char mainMenu()
{
    return tui::menu(
        "MAIN MENU",
        {"Home"},
        {
            {'1', "Emergency Room", "triage and urgent care"},
            {'2', "Appointments", "schedule and manage visits"},
            {'3', "Patient Records", "register, edit, search patients"},
            {'4', "Doctor Records", "manage staff and schedules"},
            {'5', "Ward & Beds", "admissions and bed management"},
            {'6', "Pharmacy", "medicines and inventory"},
            {'7', "Billing", "invoices and payments"},
            {'8', "Ambulance Dispatch", "emergency routing"},
            {'9', "Reports", "hospital-wide analytics"},
            {'Q', "Quit", "save and exit"},
        });
}

static void runMainLoop(Hospital &hospital)
{
    while (true)
    {
        char choice = mainMenu();
        try
        {
            switch (choice)
            {
            case '1':
                triage::run(hospital);
                break;
            case '2':
                scheduling::run(hospital);
                break;
            case '3':
                patients::run(hospital);
                break;
            case '4':
                doctors::run(hospital);
                break;
            case '5':
                wards::run(hospital);
                break;
            case '6':
                pharmacy::run(hospital);
                break;
            case '7':
                billing::run(hospital);
                break;
            case '8':
                dispatch::run(hospital);
                break;
            case '9':
                reports::run(hospital);
                break;
            case 'Q':
                hospital.saveAll();
                return;
            }
            hospital.saveAll();
        }
        catch (const CancelledException &)
        {
            hospital.saveAll();
            tui::toast("Cancelled.", tui::Level::Info);
        }
        catch (const HospitalException &e)
        {
            tui::toast(e.what(), tui::Level::Error);
            tui::pause();
        }
        catch (const std::exception &e)
        {
            tui::toast(std::string("Unexpected error: ") + e.what(),
                       tui::Level::Error);
            tui::pause();
        }
    }
}

int main(int argc, char *argv[])
{
    std::string dataDir = "data";
    if (argc > 1) {
        dataDir = argv[1];
        if (dataDir.find("..") != std::string::npos || dataDir[0] == '/') {
            std::cerr << "Error: data directory must be a relative path without '..'\n";
            return 1;
        }
    }

    Hospital hospital(dataDir);
    g_hospital = &hospital;

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    try
    {
        hospital.loadAll();
    }
    catch (const HospitalException &e)
    {
        std::cerr << "Failed to load data: " << e.what() << "\n";
        return 1;
    }

    runMainLoop(hospital);
    g_hospital = nullptr;
    return 0;
}
