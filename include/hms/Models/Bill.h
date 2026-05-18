#ifndef HMS_MODELS_BILL_H
#define HMS_MODELS_BILL_H

#include <ctime>
#include <string>
#include <vector>

namespace hms {

enum class ChargeCategory { Room, Procedure, Medicine, DoctorFee };
enum class BillStatus { Unpaid, Paid, Voided };

struct BillItem {
    ChargeCategory category;
    std::string description;
    int quantity;
    double unitPrice;

    double subtotal() const { return quantity * unitPrice; }
};

class Bill {
    int id_;
    int patientId_;
    std::vector<BillItem> items_;
    std::string discountApplied_;
    double subtotal_;
    double discountAmount_;
    double total_;
    std::time_t date_;
    BillStatus status_;

public:
    Bill() : id_(0), patientId_(0),
             subtotal_(0.0), discountAmount_(0.0), total_(0.0), date_(0),
             status_(BillStatus::Unpaid) {}

    Bill(int id, int patientId, std::time_t date)
        : id_(id), patientId_(patientId),
          subtotal_(0.0), discountAmount_(0.0), total_(0.0), date_(date),
          status_(BillStatus::Unpaid) {}

    int id() const { return id_; }
    int patientId() const { return patientId_; }
    const std::vector<BillItem>& items() const { return items_; }
    const std::string& discountApplied() const { return discountApplied_; }
    double subtotal() const { return subtotal_; }
    double discountAmount() const { return discountAmount_; }
    double total() const { return total_; }
    std::time_t date() const { return date_; }
    BillStatus status() const { return status_; }

    void setId(int value) { id_ = value; }
    void setStatus(BillStatus value) { status_ = value; }

    void addItem(const BillItem& item) {
        items_.push_back(item);
        subtotal_ += item.subtotal();
        total_    = subtotal_ - discountAmount_;
    }
    void applyDiscount(const std::string& name, double amount) {
        discountApplied_ = name;
        discountAmount_  = amount;
        total_           = subtotal_ - discountAmount_;
    }

    static std::string csvHeader();
    std::string toCSV() const;
    static Bill fromCSV(const std::string& line);
};

std::string toString(ChargeCategory category);

}

#endif
