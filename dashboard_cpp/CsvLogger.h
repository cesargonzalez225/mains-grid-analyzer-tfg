#pragma once

#include <string>
#include <fstream>
#include "ProtocolParser.h"

class CsvLogger {
public:
    CsvLogger();

    void log(const ParsedEvent& ev);

private:
    static std::string csvEscape(const std::string& field);

    std::ofstream file_;
};
