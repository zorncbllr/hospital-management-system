#ifndef HMS_CORE_REPOSITORY_H
#define HMS_CORE_REPOSITORY_H

#include <hms/Core/Exceptions.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace hms {

inline bool fileExists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

template <typename T>
class Repository {
public:
    static std::vector<T> loadAll(const std::string& path) {
        std::vector<T> records;
        std::ifstream in(path);
        if (!in) {
            return records;
        }
        std::string header;
        if (!std::getline(in, header)) return records;
        std::string line;
        int lineNumber = 1;
        int skipped = 0;
        while (std::getline(in, line)) {
            ++lineNumber;
            if (line.empty()) continue;
            try {
                records.push_back(T::fromCSV(line));
            } catch (const FileException& e) {
                std::cerr << "Warning: skipping " << path
                          << " line " << lineNumber << ": " << e.what() << "\n";
                ++skipped;
            }
        }
        if (skipped > 0) {
            std::cerr << "Warning: " << skipped << " record(s) skipped in " << path << "\n";
        }
        return records;
    }

    static void saveAll(const std::string& path,
                        const std::vector<T>& records) {
        std::string tempPath = path + ".tmp";
        {
            std::ofstream out(tempPath);
            if (!out)
                throw FileException("cannot open " + tempPath + " for writing");
            out << T::csvHeader() << "\n";
            for (const auto& record : records) {
                out << record.toCSV() << "\n";
            }
            out.flush();
            if (!out)
                throw FileException("write failed for " + tempPath);
        }
#ifdef _WIN32
        std::remove(path.c_str());
#endif
        if (std::rename(tempPath.c_str(), path.c_str()) != 0) {
            std::remove(tempPath.c_str());
            throw FileException("could not rename " + tempPath + " to " + path +
                                ": " + std::strerror(errno));
        }
    }
};

}

#endif
