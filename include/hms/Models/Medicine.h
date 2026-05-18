#ifndef HMS_MODELS_MEDICINE_H
#define HMS_MODELS_MEDICINE_H

#include <ctime>
#include <string>

namespace hms {

class Medicine {
    std::string sku_;
    std::string name_;
    int stock_;
    int reorderLevel_;
    double unitPrice_;
    std::time_t expiry_;

public:
    Medicine()
        : stock_(0), reorderLevel_(10), unitPrice_(0.0), expiry_(0) {}

    Medicine(std::string sku, std::string name,
             int stock, int reorderLevel,
             double unitPrice, std::time_t expiry)
        : sku_(std::move(sku)), name_(std::move(name)),
          stock_(stock), reorderLevel_(reorderLevel),
          unitPrice_(unitPrice), expiry_(expiry) {}

    const std::string& sku() const { return sku_; }
    const std::string& name() const { return name_; }
    int stock() const { return stock_; }
    int reorderLevel() const { return reorderLevel_; }
    double unitPrice() const { return unitPrice_; }
    std::time_t expiry() const { return expiry_; }

    void setStock(int value) { stock_ = value; }
    void setReorderLevel(int value) { reorderLevel_ = value; }
    void setUnitPrice(double value) { unitPrice_ = value; }
    void setExpiry(std::time_t when) { expiry_ = when; }
    void setName(const std::string& value) { name_ = value; }

    void restock(int amount) { stock_ += amount; }
    void dispense(int amount) { stock_ -= amount; }

    bool isLowStock() const { return stock_ <= reorderLevel_; }
    bool isExpired(std::time_t referenceDay) const {
        return expiry_ < referenceDay;
    }
    int daysUntilExpiry(std::time_t referenceDay) const {
        return static_cast<int>((expiry_ - referenceDay) / 86400);
    }

    static std::string csvHeader();
    std::string toCSV() const;
    static Medicine fromCSV(const std::string& line);
};

}

#endif
