#ifndef HMS_MODULES_TRIAGE_H
#define HMS_MODULES_TRIAGE_H

#include <hms/Modules/Module.h>

#include <ctime>
#include <queue>
#include <string>
#include <vector>

namespace hms {

class TriageModule : public Module {
public:
    using Module::Module;

    void run() override;
    char        menuKey()   const override { return '1'; }
    std::string menuLabel() const override { return "Emergency Room"; }
    std::string menuHint()  const override { return "triage and urgent care"; }

private:
    struct TriageEntry {
        int patientId;
        int severity;
        std::time_t arrival;
    };
    struct HigherPriorityFirst {
        bool operator()(const TriageEntry& a, const TriageEntry& b) const {
            if (a.severity != b.severity) return a.severity < b.severity;
            return a.arrival > b.arrival;
        }
    };
    using TriageQueue = std::priority_queue<
        TriageEntry, std::vector<TriageEntry>, HigherPriorityFirst>;

    TriageQueue              buildQueueFromHospital();
    std::vector<TriageEntry> snapshot(TriageQueue queue);
    void showQueue();
    void admitPatient();
    void callNextPatient();
    void changeSeverity();
};

}

#endif
