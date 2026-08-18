#include <cstdio>
#include <iostream>
#include <string>
#include <windows.h>
#include "App.h"

int main(int argc, char** argv) {

    SetProcessDPIAware();

    std::string port;

    if (argc >= 2) {
        port = argv[1];
    } else {

        std::cout << "Enter the COM port (e.g. COM5): ";
        std::getline(std::cin, port);
        if (port.empty()) {
            fprintf(stderr, "No port entered, exiting.\n");
            return 1;
        }
    }

    App app(port, argc, argv);
    app.run();
    return 0;
}
