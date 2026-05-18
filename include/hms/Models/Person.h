#ifndef HMS_MODELS_PERSON_H
#define HMS_MODELS_PERSON_H

#include <string>

namespace hms {

class Person {
protected:
    int id_;
    std::string name_;
    int age_;
    char sex_;
    std::string contact_;
    std::string address_;

public:
    Person() : id_(0), age_(0), sex_('M') {}
    Person(int id, std::string name, int age, char sex,
           std::string contact, std::string address)
        : id_(id), name_(std::move(name)), age_(age), sex_(sex),
          contact_(std::move(contact)), address_(std::move(address)) {}

    virtual ~Person() = default;

    int id() const { return id_; }
    const std::string& name() const { return name_; }
    int age() const { return age_; }
    char sex() const { return sex_; }
    const std::string& contact() const { return contact_; }
    const std::string& address() const { return address_; }

    void setId(int v) { id_ = v; }
    void setName(const std::string& v) { name_ = v; }
    void setAge(int v) { age_ = v; }
    void setSex(char v) { sex_ = v; }
    void setContact(const std::string& v) { contact_ = v; }
    void setAddress(const std::string& v) { address_ = v; }

    virtual std::string role() const = 0;
    virtual std::string summary() const = 0;
};

}

#endif
