#ifndef HMS_MODULES_SCHEDULING_H
#define HMS_MODULES_SCHEDULING_H

#include <hms/Modules/Module.h>

#include <ctime>
#include <string>
#include <vector>

namespace hms {

class Appointment;

class SchedulingModule : public Module {
public:
    using Module::Module;

    void run() override;
    char        menuKey()   const override { return '2'; }
    std::string menuLabel() const override { return "Appointments"; }
    std::string menuHint()  const override { return "schedule and manage visits"; }

private:
    static constexpr int CLINIC_OPEN_MIN  = 8 * 60;
    static constexpr int CLINIC_CLOSE_MIN = 18 * 60;

    static bool        sameDay(std::time_t a, std::time_t b);
    static bool        isBusinessDay(std::time_t day);
    static std::string dayOfWeekLabel(std::time_t day);
    static bool        hasConflict(const std::vector<Appointment*>& existing,
                                   int startMinutes, int endMinutes);
    static int         findNextFreeSlot(const std::vector<Appointment*>& existing,
                                        int durationMinutes, int afterMinutes);

    std::vector<Appointment*> appointmentsFor(int doctorId, std::time_t day);
    std::vector<Appointment*> filterAppointments(
        const std::vector<Appointment*>& all, const std::string& query);

    void showDailySchedule();
    void bookAppointment();
    void cancelAppointment();
    void markCompleted();
    void suggestNextFreeSlot();
};

}

#endif
