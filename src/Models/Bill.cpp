#include <hms/Core/Csv.h>
#include <hms/Core/Exceptions.h>
#include <hms/Models/Bill.h>

#include <sstream>

namespace hms {

std::string toString(ChargeCategory category) {
    switch (category) {
        case ChargeCategory::Room:      return "Room";
        case ChargeCategory::Procedure: return "Procedure";
        case ChargeCategory::Medicine:  return "Medicine";
        case ChargeCategory::DoctorFee: return "DoctorFee";
    }
    return "?";
}

static ChargeCategory parseCategory(const std::string& text) {
    if (text == "Room")      return ChargeCategory::Room;
    if (text == "Procedure") return ChargeCategory::Procedure;
    if (text == "Medicine")  return ChargeCategory::Medicine;
    if (text == "DoctorFee") return ChargeCategory::DoctorFee;
    throw FileException("unknown charge category: " + text);
}

std::string Bill::csvHeader() {
    return "id|patient_id|date|subtotal|discount_name|discount_amount|total|status|items";
}

std::string Bill::toCSV() const {
    std::ostringstream out;
    std::string statusStr;
    switch (status_) {
        case BillStatus::Unpaid:  statusStr = "Unpaid";  break;
        case BillStatus::Paid:    statusStr = "Paid";    break;
        case BillStatus::Voided:  statusStr = "Voided";  break;
    }
    out << id_
        << "|" << patientId_
        << "|" << static_cast<long long>(date_)
        << "|" << subtotal_
        << "|" << CsvCodec::escape(discountApplied_)
        << "|" << discountAmount_
        << "|" << total_
        << "|" << statusStr
        << "|";
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        if (i > 0) out << ";";
        out << toString(item.category) << "#"
            << CsvCodec::escape(item.description) << "#"
            << item.quantity << "#"
            << item.unitPrice;
    }
    return out.str();
}

static std::vector<std::string> splitOn(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == delim) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

static BillStatus parseBillStatus(const std::string& text) {
    if (text == "Unpaid")  return BillStatus::Unpaid;
    if (text == "Paid")    return BillStatus::Paid;
    if (text == "Voided")  return BillStatus::Voided;
    return BillStatus::Unpaid;
}

Bill Bill::fromCSV(const std::string& line) {
    auto fields = CsvCodec::split(line);
    if (fields.size() < 8)
        throw FileException("bill row has too few fields");
    Bill b;
    try {
        b.id_              = std::stoi(fields[0]);
        b.patientId_       = std::stoi(fields[1]);
        b.date_            = static_cast<std::time_t>(std::stoll(fields[2]));
        b.subtotal_        = std::stod(fields[3]);
        b.discountApplied_ = fields[4];
        b.discountAmount_  = std::stod(fields[5]);
        b.total_           = std::stod(fields[6]);

        if (fields.size() >= 9) {
            b.status_ = parseBillStatus(fields[7]);
        } else {
            b.status_ = BillStatus::Unpaid;
        }

        int itemsFieldIndex = (fields.size() >= 9) ? 8 : 7;
        if (!fields[itemsFieldIndex].empty()) {
            for (const auto& itemText : splitOn(fields[itemsFieldIndex], ';')) {
                auto parts = splitOn(itemText, '#');
                if (parts.size() < 4)
                    throw FileException("bill item malformed");
                BillItem item;
                item.category    = parseCategory(parts[0]);
                item.description = parts[1];
                item.quantity    = std::stoi(parts[2]);
                item.unitPrice   = std::stod(parts[3]);
                b.items_.push_back(item);
            }
        }
    } catch (const std::exception& e) {
        throw FileException(std::string("bill parse: ") + e.what());
    }
    return b;
}

}
