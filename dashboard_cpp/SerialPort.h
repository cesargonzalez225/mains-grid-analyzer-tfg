#pragma once

#include <windows.h>
#include <string>

class SerialPort {
public:
    SerialPort() = default;
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    bool open(const std::string& portName, DWORD baudRate = 115200);
    void close();
    bool isOpen() const { return handle_ != INVALID_HANDLE_VALUE; }

    bool readLine(std::string& outLine);

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::string lineBuffer_;

    bool readMoreIntoBuffer();
};
