#ifndef HMS_APPLICATION_H
#define HMS_APPLICATION_H

#include <hms/Core/Tui.h>
#include <hms/Core/Validation.h>
#include <hms/Hospital.h>

#include <memory>
#include <string>
#include <vector>

namespace hms {

class Module;

class Application {
    Hospital  hospital_;
    Tui       tui_;
    Validator validation_;
    std::vector<std::unique_ptr<Module>> modules_;

public:
    explicit Application(std::string dataDir);
    ~Application();

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;

    void run();
    void saveOnSignal() noexcept;

private:
    void buildModules();
    char promptMainMenu();
    void dispatch(char choice);
};

}

#endif
