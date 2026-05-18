#ifndef HMS_MODULES_MODULE_H
#define HMS_MODULES_MODULE_H

#include <string>

namespace hms {

class Hospital;
class Tui;
class Validator;

class Module {
protected:
    Hospital&  hospital_;
    Tui&       tui_;
    Validator& validation_;

public:
    Module(Hospital& hospital, Tui& tui, Validator& validation)
        : hospital_(hospital), tui_(tui), validation_(validation) {}
    virtual ~Module() = default;

    Module(const Module&)            = delete;
    Module& operator=(const Module&) = delete;

    virtual void        run()             = 0;
    virtual char        menuKey()   const = 0;
    virtual std::string menuLabel() const = 0;
    virtual std::string menuHint()  const = 0;
};

}

#endif
