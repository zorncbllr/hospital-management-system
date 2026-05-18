#include <hms/Application.h>

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string dataDir = "data";
    if (argc > 1) {
        dataDir = argv[1];
        if (dataDir.find("..") != std::string::npos || dataDir[0] == '/') {
            std::cerr << "Error: data directory must be a relative path without '..'\n";
            return 1;
        }
    }

    hms::Application app(std::move(dataDir));
    app.run();
    return 0;
}
