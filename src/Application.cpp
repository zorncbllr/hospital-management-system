#include <hms/Application.h>

#include <hms/Core/Exceptions.h>
#include <hms/Modules/Billing.h>
#include <hms/Modules/Dispatch.h>
#include <hms/Modules/Doctors.h>
#include <hms/Modules/Module.h>
#include <hms/Modules/Patients.h>
#include <hms/Modules/Pharmacy.h>
#include <hms/Modules/Reports.h>
#include <hms/Modules/Scheduling.h>
#include <hms/Modules/Triage.h>
#include <hms/Modules/Wards.h>

#include <csignal>
#include <iostream>

namespace hms {

namespace {
Application* g_app = nullptr;

extern "C" void onSignal(int) {
    if (g_app) g_app->saveOnSignal();
    std::exit(0);
}
}

Application::Application(std::string dataDir)
    : hospital_(std::move(dataDir)),
      validation_(tui_) {
    buildModules();
}

Application::~Application() = default;

void Application::buildModules() {
    modules_.push_back(std::make_unique<TriageModule>(hospital_, tui_, validation_));
    modules_.push_back(std::make_unique<SchedulingModule>(hospital_, tui_, validation_));
    modules_.push_back(std::make_unique<PatientsModule>(hospital_, tui_, validation_));
    modules_.push_back(std::make_unique<DoctorsModule>(hospital_, tui_, validation_));
    modules_.push_back(std::make_unique<WardsModule>(hospital_, tui_, validation_));
    modules_.push_back(std::make_unique<PharmacyModule>(hospital_, tui_, validation_));
    modules_.push_back(std::make_unique<BillingModule>(hospital_, tui_, validation_));
    modules_.push_back(std::make_unique<DispatchModule>(hospital_, tui_, validation_));
    modules_.push_back(std::make_unique<ReportsModule>(hospital_, tui_, validation_));
}

void Application::saveOnSignal() noexcept {
    try {
        hospital_.saveAll();
    } catch (const std::exception& e) {
        std::cerr << "Warning: failed to save data on exit: " << e.what() << "\n";
    }
}

char Application::promptMainMenu() {
    std::vector<Tui::MenuOption> options;
    options.reserve(modules_.size() + 1);
    for (const auto& m : modules_) {
        options.push_back({ m->menuKey(), m->menuLabel(), m->menuHint() });
    }
    options.push_back({ 'Q', "Quit", "save and exit" });
    return tui_.menu("MAIN MENU", {"Home"}, options);
}

void Application::dispatch(char choice) {
    for (const auto& m : modules_) {
        if (m->menuKey() == choice) {
            m->run();
            return;
        }
    }
}

void Application::run() {
    g_app = this;
    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    try {
        hospital_.loadAll();
    } catch (const HospitalException& e) {
        std::cerr << "Failed to load data: " << e.what() << "\n";
        g_app = nullptr;
        return;
    }

    while (true) {
        char choice = promptMainMenu();
        try {
            if (choice == 'Q') {
                hospital_.saveAll();
                break;
            }
            dispatch(choice);
            hospital_.saveAll();
        } catch (const CancelledException&) {
            hospital_.saveAll();
            tui_.toast("Cancelled.", Tui::Level::Info);
        } catch (const HospitalException& e) {
            tui_.toast(e.what(), Tui::Level::Error);
            tui_.pause();
        } catch (const std::exception& e) {
            tui_.toast(std::string("Unexpected error: ") + e.what(),
                       Tui::Level::Error);
            tui_.pause();
        }
    }

    g_app = nullptr;
}

}
