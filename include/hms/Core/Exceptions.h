#ifndef HMS_CORE_EXCEPTIONS_H
#define HMS_CORE_EXCEPTIONS_H

#include <stdexcept>
#include <string>

namespace hms {

class HospitalException : public std::runtime_error {
public:
    explicit HospitalException(const std::string& msg)
        : std::runtime_error(msg) {}
};

class InvalidInputException : public HospitalException {
public:
    explicit InvalidInputException(const std::string& msg)
        : HospitalException("Invalid input: " + msg) {}
};

class NotFoundException : public HospitalException {
public:
    explicit NotFoundException(const std::string& msg)
        : HospitalException("Not found: " + msg) {}
};

class CapacityException : public HospitalException {
public:
    explicit CapacityException(const std::string& msg)
        : HospitalException("Capacity exceeded: " + msg) {}
};

class FileException : public HospitalException {
public:
    explicit FileException(const std::string& msg)
        : HospitalException("File error: " + msg) {}
};

class CancelledException : public HospitalException {
public:
    CancelledException() : HospitalException("Operation cancelled by user") {}
};

}

#endif
